package gps

import (
	"fmt"
	"math"
	"sort"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// CellularLocationFuser implements intelligent fusion of cellular tower locations
type CellularLocationFuser struct {
	logger *logx.Logger
	config *OpenCellIDGPSConfig
}

// CellularLocation represents the fused cellular location result
type CellularLocation struct {
	Latitude      float64   `json:"latitude"`
	Longitude     float64   `json:"longitude"`
	Accuracy      float64   `json:"accuracy"`                 // Estimated accuracy in meters
	Confidence    float64   `json:"confidence"`               // Overall confidence 0.0-1.0
	Method        string    `json:"method"`                   // "single_cell", "triangulation", "weighted_centroid"
	CellCount     int       `json:"cell_count"`               // Number of cells used
	ServingCell   string    `json:"serving_cell"`             // Serving cell identifier
	TimingAdvance *float64  `json:"timing_advance,omitempty"` // TA distance constraint in meters
	Spread        float64   `json:"spread"`                   // Geographic spread of towers in meters
	FromCache     bool      `json:"from_cache"`               // Whether any data came from cache
	APICallMade   bool      `json:"api_call_made"`            // Whether API was called
	APICost       float64   `json:"api_cost"`                 // Estimated API cost
	FusedAt       time.Time `json:"fused_at"`
}

// CellularMetrics represents cellular signal metrics for fusion
type CellularMetrics struct {
	RSRP          *int     `json:"rsrp,omitempty"`           // Reference Signal Received Power
	RSRQ          *int     `json:"rsrq,omitempty"`           // Reference Signal Received Quality
	SINR          *int     `json:"sinr,omitempty"`           // Signal to Interference plus Noise Ratio
	TimingAdvance *int     `json:"timing_advance,omitempty"` // Timing Advance (0-1282)
	ServingRSRP   *int     `json:"serving_rsrp,omitempty"`   // Serving cell RSRP
	Temperature   *float64 `json:"temperature,omitempty"`    // Modem temperature
}

// WeightedTower represents a tower with calculated weight for fusion
type WeightedTower struct {
	Tower  TowerLocation
	Weight float64
	RSRP   *int // Signal strength if available
}

// NewCellularLocationFuser creates a new cellular location fuser
func NewCellularLocationFuser(config *OpenCellIDGPSConfig, logger *logx.Logger) *CellularLocationFuser {
	return &CellularLocationFuser{
		logger: logger,
		config: config,
	}
}

// FuseLocations fuses multiple cell tower locations into a single position estimate
func (clf *CellularLocationFuser) FuseLocations(towers []TowerLocation, servingCell *ServingCellInfo, metrics *CellularMetrics) (*CellularLocation, error) {
	if len(towers) == 0 {
		return nil, fmt.Errorf("no towers to fuse")
	}

	start := time.Now()

	// Determine fusion method based on available towers
	var method string
	var lat, lon, accuracy, confidence float64
	var timingAdvanceDistance *float64

	if len(towers) == 1 {
		// Single cell positioning
		method = "single_cell"
		tower := towers[0]
		lat = tower.Latitude
		lon = tower.Longitude
		accuracy = tower.Range
		confidence = tower.Confidence * 0.7 // Reduce confidence for single cell

		clf.logger.LogDebugVerbose("cellular_fusion_single_cell", map[string]interface{}{
			"cell_id":    tower.CellID,
			"latitude":   lat,
			"longitude":  lon,
			"accuracy":   accuracy,
			"confidence": confidence,
		})
	} else {
		// Multi-cell triangulation/weighted centroid
		method = "weighted_centroid"
		if len(towers) >= 3 {
			method = "triangulation"
		}

		lat, lon, accuracy, confidence = clf.calculateWeightedCentroid(towers, servingCell, metrics)

		clf.logger.LogDebugVerbose("cellular_fusion_multi_cell", map[string]interface{}{
			"method":     method,
			"cell_count": len(towers),
			"latitude":   lat,
			"longitude":  lon,
			"accuracy":   accuracy,
			"confidence": confidence,
		})
	}

	// Apply timing advance constraint if available
	if clf.config.TimingAdvanceEnabled && metrics != nil && metrics.TimingAdvance != nil {
		taDistance := clf.calculateTimingAdvanceDistance(*metrics.TimingAdvance)
		timingAdvanceDistance = &taDistance

		// Adjust accuracy based on TA constraint
		if taDistance > 0 {
			// TA provides a distance constraint to the serving cell
			accuracy = math.Min(accuracy, taDistance*2) // TA gives radius, so diameter is constraint
			confidence = math.Min(confidence*1.1, 0.95) // Slight confidence boost for TA

			clf.logger.LogDebugVerbose("cellular_fusion_timing_advance", map[string]interface{}{
				"ta_value":          *metrics.TimingAdvance,
				"ta_distance":       taDistance,
				"adjusted_accuracy": accuracy,
			})
		}
	}

	// Calculate geographic spread of towers
	spread := clf.calculateTowerSpread(towers)

	// Determine cache and API usage
	fromCache := false
	apiCallMade := false
	apiCost := 0.0

	for _, tower := range towers {
		if tower.Source == "cache" {
			fromCache = true
		} else if tower.Source == "opencellid" {
			apiCallMade = true
			apiCost += 1.0 // Assume 1 credit per API call
		}
	}

	// Create result
	result := &CellularLocation{
		Latitude:      lat,
		Longitude:     lon,
		Accuracy:      accuracy,
		Confidence:    confidence,
		Method:        method,
		CellCount:     len(towers),
		ServingCell:   fmt.Sprintf("%s-%s-%s-%s", servingCell.MCC, servingCell.MNC, servingCell.TAC, servingCell.CellID),
		TimingAdvance: timingAdvanceDistance,
		Spread:        spread,
		FromCache:     fromCache,
		APICallMade:   apiCallMade,
		APICost:       apiCost,
		FusedAt:       time.Now(),
	}

	clf.logger.Info("cellular_location_fused",
		"method", method,
		"cell_count", len(towers),
		"latitude", lat,
		"longitude", lon,
		"accuracy", accuracy,
		"confidence", confidence,
		"spread", spread,
		"timing_advance", timingAdvanceDistance,
		"fusion_time_ms", time.Since(start).Milliseconds(),
	)

	return result, nil
}

// calculateWeightedCentroid calculates weighted centroid of multiple towers
func (clf *CellularLocationFuser) calculateWeightedCentroid(towers []TowerLocation, servingCell *ServingCellInfo, metrics *CellularMetrics) (lat, lon, accuracy, confidence float64) {
	// Create weighted towers with signal-based weighting
	weightedTowers := clf.calculateTowerWeights(towers, servingCell, metrics)

	// Sort by weight (highest first)
	sort.Slice(weightedTowers, func(i, j int) bool {
		return weightedTowers[i].Weight > weightedTowers[j].Weight
	})

	// Limit to top towers to avoid noise
	maxTowers := 5
	if len(weightedTowers) > maxTowers {
		weightedTowers = weightedTowers[:maxTowers]
	}

	// Calculate weighted centroid on unit sphere (proper geodesic math)
	var totalWeight float64
	var x, y, z float64

	for _, wt := range weightedTowers {
		// Convert to Cartesian coordinates
		latRad := wt.Tower.Latitude * math.Pi / 180
		lonRad := wt.Tower.Longitude * math.Pi / 180

		cosLat := math.Cos(latRad)

		// Weighted Cartesian coordinates
		x += wt.Weight * cosLat * math.Cos(lonRad)
		y += wt.Weight * cosLat * math.Sin(lonRad)
		z += wt.Weight * math.Sin(latRad)

		totalWeight += wt.Weight
	}

	if totalWeight == 0 {
		// Fallback to simple average
		return clf.calculateSimpleAverage(towers)
	}

	// Normalize
	x /= totalWeight
	y /= totalWeight
	z /= totalWeight

	// Convert back to lat/lon
	lat = math.Atan2(z, math.Sqrt(x*x+y*y)) * 180 / math.Pi
	lon = math.Atan2(y, x) * 180 / math.Pi

	// Calculate accuracy estimate
	accuracy = clf.calculateAccuracyEstimate(weightedTowers)

	// Calculate confidence based on tower quality and spread
	confidence = clf.calculateFusionConfidence(weightedTowers)

	clf.logger.LogDebugVerbose("cellular_weighted_centroid", map[string]interface{}{
		"towers_used":  len(weightedTowers),
		"total_weight": totalWeight,
		"latitude":     lat,
		"longitude":    lon,
		"accuracy":     accuracy,
		"confidence":   confidence,
	})

	return lat, lon, accuracy, confidence
}

// calculateTowerWeights calculates weights for each tower based on signal strength and accuracy
func (clf *CellularLocationFuser) calculateTowerWeights(towers []TowerLocation, servingCell *ServingCellInfo, metrics *CellularMetrics) []WeightedTower {
	var weighted []WeightedTower

	for _, tower := range towers {
		weight := 1.0 / (tower.Range * tower.Range) // Inverse square of range

		// Boost weight based on sample count
		if tower.Samples > 0 {
			sampleFactor := math.Log10(float64(tower.Samples) + 1) // Logarithmic boost
			weight *= (1.0 + sampleFactor*0.2)
		}

		// Boost weight based on confidence
		weight *= tower.Confidence

		// Check if this is the serving cell and boost accordingly
		isServingCell := (tower.CellID.MCC == servingCell.MCC &&
			tower.CellID.MNC == servingCell.MNC &&
			tower.CellID.LAC == servingCell.TAC &&
			tower.CellID.CellID == servingCell.CellID)

		if isServingCell {
			weight *= 2.0 // Double weight for serving cell
		}

		// Apply RSRP-based weighting if available
		var rsrp *int
		if metrics != nil && isServingCell && metrics.ServingRSRP != nil {
			rsrp = metrics.ServingRSRP
		}

		if rsrp != nil && *rsrp < 0 {
			// Convert RSRP to linear scale and apply weighting
			// RSRP ranges from about -140 to -44 dBm
			rsrpLinear := math.Pow(10, float64(*rsrp)/10) // Convert dBm to linear
			weight *= rsrpLinear / 1e-12                  // Normalize to reasonable range
		}

		weighted = append(weighted, WeightedTower{
			Tower:  tower,
			Weight: weight,
			RSRP:   rsrp,
		})
	}

	return weighted
}

// calculateAccuracyEstimate estimates the accuracy of the fused position
func (clf *CellularLocationFuser) calculateAccuracyEstimate(weightedTowers []WeightedTower) float64 {
	if len(weightedTowers) == 0 {
		return 10000 // Very poor accuracy
	}

	// Use the minimum range of the towers as base accuracy
	minRange := weightedTowers[0].Tower.Range
	for _, wt := range weightedTowers {
		if wt.Tower.Range < minRange {
			minRange = wt.Tower.Range
		}
	}

	// Calculate spread-based accuracy adjustment
	spread := clf.calculateWeightedTowerSpread(weightedTowers)

	// Conservative accuracy estimate: max of 2x minimum range or spread sigma
	accuracy := math.Max(2*minRange, spread*0.5)

	// Improve accuracy with more towers (diminishing returns)
	towerFactor := 1.0 / math.Sqrt(float64(len(weightedTowers)))
	accuracy *= towerFactor

	// Ensure reasonable bounds
	if accuracy < 50 {
		accuracy = 50 // Minimum 50m accuracy for cellular
	}
	if accuracy > 10000 {
		accuracy = 10000 // Maximum 10km accuracy
	}

	return accuracy
}

// calculateFusionConfidence calculates confidence based on tower quality and geometry
func (clf *CellularLocationFuser) calculateFusionConfidence(weightedTowers []WeightedTower) float64 {
	if len(weightedTowers) == 0 {
		return 0.1
	}

	// Base confidence from tower confidence (weighted average)
	var totalWeight, weightedConfidence float64
	for _, wt := range weightedTowers {
		totalWeight += wt.Weight
		weightedConfidence += wt.Weight * wt.Tower.Confidence
	}

	baseConfidence := weightedConfidence / totalWeight

	// Boost confidence with more towers
	towerBoost := 1.0 + (float64(len(weightedTowers))-1)*0.1
	if towerBoost > 1.5 {
		towerBoost = 1.5 // Cap at 50% boost
	}

	// Boost confidence based on geometric diversity
	spread := clf.calculateWeightedTowerSpread(weightedTowers)
	geometryBoost := 1.0
	if spread > 1000 { // Good geometric diversity
		geometryBoost = 1.2
	} else if spread < 200 { // Poor geometric diversity
		geometryBoost = 0.8
	}

	confidence := baseConfidence * towerBoost * geometryBoost

	// Ensure bounds
	if confidence > 0.95 {
		confidence = 0.95
	}
	if confidence < 0.1 {
		confidence = 0.1
	}

	return confidence
}

// calculateSimpleAverage calculates simple average as fallback
func (clf *CellularLocationFuser) calculateSimpleAverage(towers []TowerLocation) (lat, lon, accuracy, confidence float64) {
	var totalLat, totalLon, totalRange, totalConfidence float64

	for _, tower := range towers {
		totalLat += tower.Latitude
		totalLon += tower.Longitude
		totalRange += tower.Range
		totalConfidence += tower.Confidence
	}

	count := float64(len(towers))
	lat = totalLat / count
	lon = totalLon / count
	accuracy = totalRange / count
	confidence = (totalConfidence / count) * 0.8 // Reduce confidence for simple average

	return lat, lon, accuracy, confidence
}

// calculateTowerSpread calculates the geographic spread of towers
func (clf *CellularLocationFuser) calculateTowerSpread(towers []TowerLocation) float64 {
	if len(towers) < 2 {
		return 0
	}

	// Calculate centroid
	var centerLat, centerLon float64
	for _, tower := range towers {
		centerLat += tower.Latitude
		centerLon += tower.Longitude
	}
	centerLat /= float64(len(towers))
	centerLon /= float64(len(towers))

	// Calculate distances from centroid
	var totalDistance float64
	for _, tower := range towers {
		distance := clf.haversineDistance(centerLat, centerLon, tower.Latitude, tower.Longitude)
		totalDistance += distance * distance // Sum of squares
	}

	// Return standard deviation of distances
	variance := totalDistance / float64(len(towers))
	return math.Sqrt(variance)
}

// calculateWeightedTowerSpread calculates weighted geographic spread
func (clf *CellularLocationFuser) calculateWeightedTowerSpread(weightedTowers []WeightedTower) float64 {
	if len(weightedTowers) < 2 {
		return 0
	}

	// Calculate weighted centroid
	var centerLat, centerLon, totalWeight float64
	for _, wt := range weightedTowers {
		centerLat += wt.Weight * wt.Tower.Latitude
		centerLon += wt.Weight * wt.Tower.Longitude
		totalWeight += wt.Weight
	}
	centerLat /= totalWeight
	centerLon /= totalWeight

	// Calculate weighted distances from centroid
	var weightedVariance float64
	for _, wt := range weightedTowers {
		distance := clf.haversineDistance(centerLat, centerLon, wt.Tower.Latitude, wt.Tower.Longitude)
		weightedVariance += wt.Weight * distance * distance
	}

	// Return weighted standard deviation
	weightedVariance /= totalWeight
	return math.Sqrt(weightedVariance)
}

// calculateTimingAdvanceDistance converts timing advance to distance
func (clf *CellularLocationFuser) calculateTimingAdvanceDistance(ta int) float64 {
	// Timing Advance (TA) in LTE:
	// - TA value range: 0-1282
	// - Each TA unit = 78.125 meters (speed of light * 0.52 μs / 2)
	// - Distance = TA * 78.125 meters

	if ta < 0 || ta > 1282 {
		return 0 // Invalid TA value
	}

	return float64(ta) * 78.125
}

// haversineDistance calculates distance between two points using Haversine formula
func (clf *CellularLocationFuser) haversineDistance(lat1, lon1, lat2, lon2 float64) float64 {
	const earthRadiusM = 6371000 // Earth's radius in meters

	lat1Rad := lat1 * math.Pi / 180
	lat2Rad := lat2 * math.Pi / 180
	deltaLatRad := (lat2 - lat1) * math.Pi / 180
	deltaLonRad := (lon2 - lon1) * math.Pi / 180

	a := math.Sin(deltaLatRad/2)*math.Sin(deltaLatRad/2) +
		math.Cos(lat1Rad)*math.Cos(lat2Rad)*
			math.Sin(deltaLonRad/2)*math.Sin(deltaLonRad/2)
	c := 2 * math.Atan2(math.Sqrt(a), math.Sqrt(1-a))

	return earthRadiusM * c
}
