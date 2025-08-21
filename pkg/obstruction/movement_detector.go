package obstruction

import (
	"context"
	"fmt"
	"math"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// MovementDetector detects movement and triggers obstruction map refresh
type MovementDetector struct {
	mu     sync.RWMutex
	logger *logx.Logger
	config *MovementDetectorConfig

	// Location tracking
	lastKnownLocation *LocationInfo
	locationHistory   []LocationPoint

	// Movement state
	isMoving          bool
	movementStartTime *time.Time
	lastMovementTime  *time.Time
	totalDistance     float64

	// Callbacks
	onMovementStart func(context.Context, *LocationInfo) error
	onMovementEnd   func(context.Context, *LocationInfo, time.Duration, float64) error
}

// LocationPoint represents a timestamped location
type LocationPoint struct {
	Location  LocationInfo `json:"location"`
	Timestamp time.Time    `json:"timestamp"`
	Speed     float64      `json:"speed"`    // Speed in m/s
	Bearing   float64      `json:"bearing"`  // Bearing in degrees
	Accuracy  float64      `json:"accuracy"` // GPS accuracy in meters
}

// MovementDetectorConfig holds configuration for movement detection
type MovementDetectorConfig struct {
	MinMovementDistance    float64       `json:"min_movement_distance"`    // Minimum distance to consider movement (meters)
	MovementTimeout        time.Duration `json:"movement_timeout"`         // Time without movement to consider stopped
	LocationHistorySize    int           `json:"location_history_size"`    // Number of location points to keep
	MinAccuracyMeters      float64       `json:"min_accuracy_meters"`      // Minimum GPS accuracy to trust (meters)
	SpeedSmoothingWindow   int           `json:"speed_smoothing_window"`   // Number of points for speed smoothing
	MovementSpeedThreshold float64       `json:"movement_speed_threshold"` // Minimum speed to consider moving (m/s)
	StationaryTimeRequired time.Duration `json:"stationary_time_required"` // Time required to be stationary before triggering
	SignificantDistance    float64       `json:"significant_distance"`     // Distance that triggers obstruction refresh (meters)
}

// DefaultMovementDetectorConfig returns default configuration
func DefaultMovementDetectorConfig() *MovementDetectorConfig {
	return &MovementDetectorConfig{
		MinMovementDistance:    10.0, // 10 meters
		MovementTimeout:        5 * time.Minute,
		LocationHistorySize:    100,  // Keep last 100 location points
		MinAccuracyMeters:      20.0, // Trust GPS within 20 meters
		SpeedSmoothingWindow:   5,    // Smooth over 5 points
		MovementSpeedThreshold: 1.0,  // 1 m/s minimum speed
		StationaryTimeRequired: 2 * time.Minute,
		SignificantDistance:    50.0, // 50 meters triggers refresh
	}
}

// NewMovementDetector creates a new movement detector
func NewMovementDetector(logger *logx.Logger) *MovementDetector {
	config := DefaultMovementDetectorConfig()

	return &MovementDetector{
		logger:          logger,
		config:          config,
		locationHistory: make([]LocationPoint, 0, config.LocationHistorySize),
	}
}

// SetMovementCallbacks sets callbacks for movement events
func (md *MovementDetector) SetMovementCallbacks(
	onStart func(context.Context, *LocationInfo) error,
	onEnd func(context.Context, *LocationInfo, time.Duration, float64) error,
) {
	md.mu.Lock()
	defer md.mu.Unlock()

	md.onMovementStart = onStart
	md.onMovementEnd = onEnd
}

// UpdateLocation updates the current location and detects movement
func (md *MovementDetector) UpdateLocation(ctx context.Context, location *LocationInfo) error {
	md.mu.Lock()
	defer md.mu.Unlock()

	now := time.Now()

	// Validate location accuracy
	if location.Accuracy > md.config.MinAccuracyMeters {
		md.logger.Debug("Ignoring location update due to poor accuracy",
			"accuracy", location.Accuracy,
			"threshold", md.config.MinAccuracyMeters)
		return nil
	}

	// Create location point
	point := LocationPoint{
		Location:  *location,
		Timestamp: now,
		Accuracy:  location.Accuracy,
	}

	// Calculate speed and bearing if we have previous location
	if len(md.locationHistory) > 0 {
		lastPoint := md.locationHistory[len(md.locationHistory)-1]
		distance := md.haversineDistance(
			lastPoint.Location.Latitude, lastPoint.Location.Longitude,
			location.Latitude, location.Longitude)

		timeDiff := now.Sub(lastPoint.Timestamp).Seconds()
		if timeDiff > 0 {
			point.Speed = distance / timeDiff
			point.Bearing = md.calculateBearing(
				lastPoint.Location.Latitude, lastPoint.Location.Longitude,
				location.Latitude, location.Longitude)
		}
	}

	// Add to history
	md.locationHistory = append(md.locationHistory, point)
	if len(md.locationHistory) > md.config.LocationHistorySize {
		md.locationHistory = md.locationHistory[1:]
	}

	// Update last known location
	md.lastKnownLocation = location

	// Detect movement state changes
	if err := md.detectMovementStateChange(ctx, point); err != nil {
		return fmt.Errorf("failed to detect movement state change: %w", err)
	}

	md.logger.Debug("Updated location",
		"lat", location.Latitude,
		"lon", location.Longitude,
		"accuracy", location.Accuracy,
		"speed", point.Speed,
		"is_moving", md.isMoving)

	return nil
}

// detectMovementStateChange detects changes in movement state
func (md *MovementDetector) detectMovementStateChange(ctx context.Context, point LocationPoint) error {
	now := time.Now()

	// Calculate smoothed speed
	smoothedSpeed := md.calculateSmoothedSpeed()

	// Determine if currently moving based on speed
	currentlyMoving := smoothedSpeed > md.config.MovementSpeedThreshold

	// Check for movement start
	if !md.isMoving && currentlyMoving {
		md.isMoving = true
		md.movementStartTime = &now
		md.lastMovementTime = &now
		md.totalDistance = 0

		md.logger.Info("Movement started",
			"location", fmt.Sprintf("%.6f,%.6f", point.Location.Latitude, point.Location.Longitude),
			"speed", smoothedSpeed)

		// Trigger movement start callback
		if md.onMovementStart != nil {
			if err := md.onMovementStart(ctx, &point.Location); err != nil {
				md.logger.Warn("Movement start callback failed", "error", err)
			}
		}

		return nil
	}

	// Check for movement end
	if md.isMoving && !currentlyMoving {
		// Require stationary time before considering movement ended
		if md.lastMovementTime != nil {
			stationaryDuration := now.Sub(*md.lastMovementTime)
			if stationaryDuration < md.config.StationaryTimeRequired {
				return nil // Not stationary long enough yet
			}
		}

		// Movement has ended
		var movementDuration time.Duration
		if md.movementStartTime != nil {
			movementDuration = now.Sub(*md.movementStartTime)
		}

		md.logger.Info("Movement ended",
			"location", fmt.Sprintf("%.6f,%.6f", point.Location.Latitude, point.Location.Longitude),
			"duration", movementDuration,
			"distance", md.totalDistance)

		// Trigger movement end callback
		if md.onMovementEnd != nil {
			if err := md.onMovementEnd(ctx, &point.Location, movementDuration, md.totalDistance); err != nil {
				md.logger.Warn("Movement end callback failed", "error", err)
			}
		}

		md.isMoving = false
		md.movementStartTime = nil
		md.lastMovementTime = nil
		md.totalDistance = 0

		return nil
	}

	// Update movement tracking if currently moving
	if md.isMoving {
		md.lastMovementTime = &now

		// Update total distance
		if len(md.locationHistory) >= 2 {
			lastPoint := md.locationHistory[len(md.locationHistory)-2]
			distance := md.haversineDistance(
				lastPoint.Location.Latitude, lastPoint.Location.Longitude,
				point.Location.Latitude, point.Location.Longitude)
			md.totalDistance += distance
		}
	}

	return nil
}

// calculateSmoothedSpeed calculates smoothed speed over recent points
func (md *MovementDetector) calculateSmoothedSpeed() float64 {
	if len(md.locationHistory) < 2 {
		return 0
	}

	// Use last N points for smoothing
	windowSize := md.config.SpeedSmoothingWindow
	if windowSize > len(md.locationHistory) {
		windowSize = len(md.locationHistory)
	}

	recentPoints := md.locationHistory[len(md.locationHistory)-windowSize:]

	var totalDistance float64
	var totalTime float64

	for i := 1; i < len(recentPoints); i++ {
		distance := md.haversineDistance(
			recentPoints[i-1].Location.Latitude, recentPoints[i-1].Location.Longitude,
			recentPoints[i].Location.Latitude, recentPoints[i].Location.Longitude)

		timeDiff := recentPoints[i].Timestamp.Sub(recentPoints[i-1].Timestamp).Seconds()

		totalDistance += distance
		totalTime += timeDiff
	}

	if totalTime == 0 {
		return 0
	}

	return totalDistance / totalTime
}

// haversineDistance calculates the distance between two points on Earth
func (md *MovementDetector) haversineDistance(lat1, lon1, lat2, lon2 float64) float64 {
	const R = 6371000 // Earth's radius in meters

	lat1Rad := lat1 * math.Pi / 180
	lat2Rad := lat2 * math.Pi / 180
	deltaLatRad := (lat2 - lat1) * math.Pi / 180
	deltaLonRad := (lon2 - lon1) * math.Pi / 180

	a := math.Sin(deltaLatRad/2)*math.Sin(deltaLatRad/2) +
		math.Cos(lat1Rad)*math.Cos(lat2Rad)*
			math.Sin(deltaLonRad/2)*math.Sin(deltaLonRad/2)
	c := 2 * math.Atan2(math.Sqrt(a), math.Sqrt(1-a))

	return R * c
}

// calculateBearing calculates the bearing from one point to another
func (md *MovementDetector) calculateBearing(lat1, lon1, lat2, lon2 float64) float64 {
	lat1Rad := lat1 * math.Pi / 180
	lat2Rad := lat2 * math.Pi / 180
	deltaLonRad := (lon2 - lon1) * math.Pi / 180

	y := math.Sin(deltaLonRad) * math.Cos(lat2Rad)
	x := math.Cos(lat1Rad)*math.Sin(lat2Rad) - math.Sin(lat1Rad)*math.Cos(lat2Rad)*math.Cos(deltaLonRad)

	bearing := math.Atan2(y, x) * 180 / math.Pi

	// Normalize to 0-360 degrees
	if bearing < 0 {
		bearing += 360
	}

	return bearing
}

// IsMoving returns whether the detector considers the device to be moving
func (md *MovementDetector) IsMoving() bool {
	md.mu.RLock()
	defer md.mu.RUnlock()

	return md.isMoving
}

// GetCurrentLocation returns the last known location
func (md *MovementDetector) GetCurrentLocation() *LocationInfo {
	md.mu.RLock()
	defer md.mu.RUnlock()

	return md.lastKnownLocation
}

// GetMovementState returns detailed movement state information
func (md *MovementDetector) GetMovementState() *MovementState {
	md.mu.RLock()
	defer md.mu.RUnlock()

	state := &MovementState{
		IsMoving:        md.isMoving,
		CurrentLocation: md.lastKnownLocation,
		TotalDistance:   md.totalDistance,
	}

	if md.movementStartTime != nil {
		state.MovementStartTime = md.movementStartTime
		state.MovementDuration = time.Since(*md.movementStartTime)
	}

	if len(md.locationHistory) > 0 {
		state.CurrentSpeed = md.calculateSmoothedSpeed()

		// Calculate recent bearing if we have enough points
		if len(md.locationHistory) >= 2 {
			recent := md.locationHistory[len(md.locationHistory)-1]
			previous := md.locationHistory[len(md.locationHistory)-2]
			state.CurrentBearing = md.calculateBearing(
				previous.Location.Latitude, previous.Location.Longitude,
				recent.Location.Latitude, recent.Location.Longitude)
		}
	}

	return state
}

// MovementState represents the current movement state
type MovementState struct {
	IsMoving          bool          `json:"is_moving"`
	CurrentLocation   *LocationInfo `json:"current_location,omitempty"`
	MovementStartTime *time.Time    `json:"movement_start_time,omitempty"`
	MovementDuration  time.Duration `json:"movement_duration,omitempty"`
	TotalDistance     float64       `json:"total_distance"`
	CurrentSpeed      float64       `json:"current_speed"`   // m/s
	CurrentBearing    float64       `json:"current_bearing"` // degrees
}

// GetLocationHistory returns recent location history
func (md *MovementDetector) GetLocationHistory(maxPoints int) []LocationPoint {
	md.mu.RLock()
	defer md.mu.RUnlock()

	if maxPoints <= 0 || maxPoints >= len(md.locationHistory) {
		// Return copy of all history
		history := make([]LocationPoint, len(md.locationHistory))
		copy(history, md.locationHistory)
		return history
	}

	// Return copy of recent history
	start := len(md.locationHistory) - maxPoints
	history := make([]LocationPoint, maxPoints)
	copy(history, md.locationHistory[start:])
	return history
}

// ShouldRefreshObstructionMap determines if obstruction map should be refreshed
func (md *MovementDetector) ShouldRefreshObstructionMap() (bool, string) {
	md.mu.RLock()
	defer md.mu.RUnlock()

	// Don't refresh if we don't have enough location data
	if len(md.locationHistory) < 2 {
		return false, "insufficient location history"
	}

	// Check if we've moved significantly since last refresh
	// This is a simplified implementation - production code might track
	// the last refresh location more precisely

	if md.totalDistance >= md.config.SignificantDistance {
		return true, fmt.Sprintf("moved %.1f meters >= %.1f threshold",
			md.totalDistance, md.config.SignificantDistance)
	}

	// Check if we've been stationary for a while after movement
	if !md.isMoving && md.lastMovementTime != nil {
		stationaryDuration := time.Since(*md.lastMovementTime)
		if stationaryDuration >= md.config.StationaryTimeRequired {
			return true, fmt.Sprintf("stationary for %v after movement", stationaryDuration)
		}
	}

	return false, "no significant movement detected"
}

// Reset resets the movement detector state
func (md *MovementDetector) Reset() {
	md.mu.Lock()
	defer md.mu.Unlock()

	md.isMoving = false
	md.movementStartTime = nil
	md.lastMovementTime = nil
	md.totalDistance = 0
	md.locationHistory = md.locationHistory[:0] // Clear but keep capacity

	md.logger.Info("Movement detector reset")
}

// GetStatus returns current detector status
func (md *MovementDetector) GetStatus() map[string]interface{} {
	md.mu.RLock()
	defer md.mu.RUnlock()

	status := map[string]interface{}{
		"is_moving":             md.isMoving,
		"total_distance":        md.totalDistance,
		"location_history_size": len(md.locationHistory),
		"config":                md.config,
	}

	if md.lastKnownLocation != nil {
		status["last_known_location"] = map[string]interface{}{
			"latitude":  md.lastKnownLocation.Latitude,
			"longitude": md.lastKnownLocation.Longitude,
			"accuracy":  md.lastKnownLocation.Accuracy,
		}
	}

	if md.movementStartTime != nil {
		status["movement_start_time"] = *md.movementStartTime
		status["movement_duration"] = time.Since(*md.movementStartTime).String()
	}

	if len(md.locationHistory) > 0 {
		status["current_speed"] = md.calculateSmoothedSpeed()
	}

	return status
}
