package gps

import (
	"time"
)

// Note: ServingCellInfo and NeighborCellInfo are defined in cellular_intelligence.go

// Note: CellTowerObservation is defined in local_cell_database.go

// CellKey represents a unique identifier for a cell tower
type CellKey struct {
	MCC    string `json:"mcc"`
	MNC    string `json:"mnc"`
	LAC    string `json:"lac"`
	CellID string `json:"cell_id"`
	Radio  string `json:"radio"`
}

// String returns a string representation of the cell key
func (ck CellKey) String() string {
	return ck.MCC + "-" + ck.MNC + "-" + ck.LAC + "-" + ck.CellID + "-" + ck.Radio
}

// OpenCellIDContribution represents a contribution to OpenCellID in their expected format
type OpenCellIDContribution struct {
	// Required fields
	MCC    int     `json:"mcc"`
	MNC    int     `json:"mnc"`
	LAC    int     `json:"lac,omitempty"`
	CellID int     `json:"cellid,omitempty"`
	Lat    float64 `json:"lat"`
	Lon    float64 `json:"lon"`

	// Optional fields
	Signal     int     `json:"signal,omitempty"`    // Signal strength in dBm
	MeasuredAt int64   `json:"measured_at"`         // Unix timestamp in milliseconds
	Rating     float64 `json:"rating,omitempty"`    // GPS accuracy in meters
	Speed      float64 `json:"speed,omitempty"`     // Speed in m/s
	Direction  float64 `json:"direction,omitempty"` // Heading in degrees
	Act        string  `json:"act"`                 // Radio technology (GSM, UMTS, LTE, NR, CDMA)

	// Technology-specific fields
	TA  int `json:"ta,omitempty"`  // Timing Advance (LTE)
	PCI int `json:"pci,omitempty"` // Physical Cell ID (LTE/NR)
	PSC int `json:"psc,omitempty"` // Primary Scrambling Code (UMTS)
	TAC int `json:"tac,omitempty"` // Tracking Area Code (LTE/NR)

	// CDMA fields
	SID int `json:"sid,omitempty"` // System ID (use MNC)
	NID int `json:"nid,omitempty"` // Network ID (use LAC)
	BID int `json:"bid,omitempty"` // Base ID (use CellID)
}

// Note: OpenCellIDResponse is defined in opencellid_api.go

// CellLocationRequest represents a request for cell tower location
type CellLocationRequest struct {
	Cells   []CellIdentifier       `json:"cells"`
	Options LocationRequestOptions `json:"options,omitempty"`
}

// LocationRequestOptions represents options for location requests
type LocationRequestOptions struct {
	IncludeNeighbors bool   `json:"include_neighbors"`
	MaxCells         int    `json:"max_cells"`
	RadioFilter      string `json:"radio_filter,omitempty"`    // GSM, UMTS, LTE, NR
	AccuracyFilter   int    `json:"accuracy_filter,omitempty"` // Max acceptable range in meters
}

// CellLocationResponse represents the response from a cell location request
type CellLocationResponse struct {
	Locations []TowerLocation `json:"locations"`
	Stats     ResponseStats   `json:"stats"`
}

// ResponseStats represents statistics about the location response
type ResponseStats struct {
	RequestedCells int           `json:"requested_cells"`
	ResolvedCells  int           `json:"resolved_cells"`
	CacheHits      int           `json:"cache_hits"`
	APICallsMade   int           `json:"api_calls_made"`
	ResponseTime   time.Duration `json:"response_time"`
	TotalCost      float64       `json:"total_cost"`
}

// ContributionRequest represents a request to contribute cell observations
type ContributionRequest struct {
	Observations []CellObservation   `json:"observations"`
	Options      ContributionOptions `json:"options,omitempty"`
}

// ContributionOptions represents options for contributions
type ContributionOptions struct {
	Format           string `json:"format"`            // "json", "csv"
	BatchSize        int    `json:"batch_size"`        // Max observations per batch
	ValidateGPS      bool   `json:"validate_gps"`      // Validate GPS accuracy
	DeduplicateCells bool   `json:"deduplicate_cells"` // Remove duplicate cell observations
}

// ContributionResponse represents the response from a contribution request
type ContributionResponse struct {
	Success          bool              `json:"success"`
	ContributedCells int               `json:"contributed_cells"`
	SkippedCells     int               `json:"skipped_cells"`
	Errors           []string          `json:"errors,omitempty"`
	Stats            ContributionStats `json:"stats"`
}

// CellularDataSource represents a source of cellular data
type CellularDataSource interface {
	// GetServingCell returns information about the serving cell
	GetServingCell() (*ServingCellInfo, error)

	// GetNeighborCells returns information about neighbor cells
	GetNeighborCells() ([]NeighborCellInfo, error)

	// GetCellularMetrics returns detailed cellular metrics
	GetCellularMetrics() (*CellularMetrics, error)

	// IsAvailable checks if the cellular data source is available
	IsAvailable() bool
}

// LocationProvider represents a provider of location services
type LocationProvider interface {
	// GetLocation resolves location for given cells
	GetLocation(cells []CellIdentifier) ([]TowerLocation, error)

	// ContributeObservation contributes an observation
	ContributeObservation(observation *CellObservation) error

	// GetProviderStats returns provider statistics
	GetProviderStats() interface{}
}

// CacheProvider represents a cache provider for cell locations
type CacheProvider interface {
	// Get retrieves a cached location
	Get(key CellKey) (*CachedCellLocation, error)

	// Set stores a location in cache
	Set(key CellKey, location *CachedCellLocation) error

	// Delete removes a location from cache
	Delete(key CellKey) error

	// GetStats returns cache statistics
	GetStats() CacheStats

	// Close closes the cache
	Close() error
}

// MovementDetector detects significant movement for contribution decisions
type MovementDetector struct {
	lastLocation      *GPSObservation
	movementThreshold float64 // meters
	lastMovementTime  time.Time
}

// NewMovementDetector creates a new movement detector
func NewMovementDetector(thresholdM float64) *MovementDetector {
	return &MovementDetector{
		movementThreshold: thresholdM,
	}
}

// DetectMovement checks if there has been significant movement
func (md *MovementDetector) DetectMovement(currentLocation GPSObservation) (bool, float64) {
	if md.lastLocation == nil {
		md.lastLocation = &currentLocation
		return true, 0 // First location is always considered movement
	}

	// Calculate distance using Haversine formula
	distance := md.calculateDistance(*md.lastLocation, currentLocation)

	if distance >= md.movementThreshold {
		md.lastLocation = &currentLocation
		md.lastMovementTime = time.Now()
		return true, distance
	}

	return false, distance
}

// calculateDistance calculates distance between two GPS points using Haversine formula
func (md *MovementDetector) calculateDistance(loc1, loc2 GPSObservation) float64 {
	const earthRadiusM = 6371000 // Earth's radius in meters

	lat1Rad := loc1.Latitude * 3.14159265359 / 180
	lat2Rad := loc2.Latitude * 3.14159265359 / 180
	deltaLat := (loc2.Latitude - loc1.Latitude) * 3.14159265359 / 180
	deltaLon := (loc2.Longitude - loc1.Longitude) * 3.14159265359 / 180

	a := 0.5*(1-((lat2Rad-lat1Rad)/2)) +
		0.5*((lat2Rad+lat1Rad)/2)*0.5*(1-((deltaLon)/2))
	_ = deltaLat // Avoid unused variable warning

	return earthRadiusM * 2 * 1.5707963268 * a // Simplified Haversine
}

// GetLastMovementTime returns the time of last detected movement
func (md *MovementDetector) GetLastMovementTime() time.Time {
	return md.lastMovementTime
}

// GetCurrentLocation returns the current stored location
func (md *MovementDetector) GetCurrentLocation() *GPSObservation {
	return md.lastLocation
}
