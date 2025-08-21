package gps

import (
	"fmt"
	"math"
	"sort"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// LocationClusterManager manages location-based performance clusters
type LocationClusterManager struct {
	logger   *logx.Logger
	config   *ClusteringConfig
	clusters []*pkg.LocationCluster
}

// ClusteringConfig represents clustering configuration
type ClusteringConfig struct {
	Enabled                bool    `json:"enabled"`
	MinClusterRadius       float64 `json:"min_cluster_radius"`       // Minimum cluster radius in meters
	MaxClusterRadius       float64 `json:"max_cluster_radius"`       // Maximum cluster radius in meters
	MinSamplesPerCluster   int     `json:"min_samples_per_cluster"`  // Minimum samples to form a cluster
	ProblematicThreshold   float64 `json:"problematic_threshold"`    // Performance threshold for problematic classification
	ClusterMergeDistance   float64 `json:"cluster_merge_distance"`   // Distance to merge clusters
	MaxClusters            int     `json:"max_clusters"`             // Maximum number of clusters to maintain
	ClusterExpiryDays      int     `json:"cluster_expiry_days"`      // Days after which clusters expire
	PerformanceWindowHours int     `json:"performance_window_hours"` // Hours to consider for performance analysis
}

// PerformanceSample represents a performance sample at a location
type PerformanceSample struct {
	Location    *pkg.GPSData `json:"location"`
	Latency     float64      `json:"latency"`
	Loss        float64      `json:"loss"`
	Obstruction float64      `json:"obstruction"`
	Timestamp   time.Time    `json:"timestamp"`
	Interface   string       `json:"interface"`
}

// NewLocationClusterManager creates a new location cluster manager
func NewLocationClusterManager(config *ClusteringConfig, logger *logx.Logger) *LocationClusterManager {
	if config == nil {
		config = DefaultClusteringConfig()
	}

	return &LocationClusterManager{
		logger:   logger,
		config:   config,
		clusters: []*pkg.LocationCluster{},
	}
}

// DefaultClusteringConfig returns default clustering configuration
func DefaultClusteringConfig() *ClusteringConfig {
	return &ClusteringConfig{
		Enabled:                true,
		MinClusterRadius:       100.0,  // 100 meters
		MaxClusterRadius:       1000.0, // 1 kilometer
		MinSamplesPerCluster:   5,      // At least 5 samples
		ProblematicThreshold:   80.0,   // Below 80% performance score
		ClusterMergeDistance:   200.0,  // 200 meters
		MaxClusters:            50,     // Maximum 50 clusters
		ClusterExpiryDays:      30,     // Expire after 30 days
		PerformanceWindowHours: 168,    // 1 week performance window
	}
}

// AddPerformanceSample adds a performance sample to the clustering system
func (lcm *LocationClusterManager) AddPerformanceSample(sample *PerformanceSample) error {
	if !lcm.config.Enabled {
		return nil
	}

	if sample.Location == nil || !sample.Location.Valid {
		return fmt.Errorf("invalid location data")
	}

	lcm.logger.LogDataFlow("location_clustering", "performance_sample", "sample", 1, map[string]interface{}{
		"latitude":    sample.Location.Latitude,
		"longitude":   sample.Location.Longitude,
		"latency":     sample.Latency,
		"loss":        sample.Loss,
		"obstruction": sample.Obstruction,
		"interface":   sample.Interface,
	})

	// Find existing cluster or create new one
	cluster := lcm.findOrCreateCluster(sample)
	if cluster == nil {
		return fmt.Errorf("failed to create cluster for sample")
	}

	// Update cluster statistics
	lcm.updateClusterStatistics(cluster, sample)

	// Check if cluster should be marked as problematic
	lcm.evaluateClusterHealth(cluster)

	// Perform cluster maintenance
	lcm.performMaintenance()

	return nil
}

// findOrCreateCluster finds an existing cluster or creates a new one for the sample
func (lcm *LocationClusterManager) findOrCreateCluster(sample *PerformanceSample) *pkg.LocationCluster {
	// Find the closest cluster within merge distance
	var closestCluster *pkg.LocationCluster
	minDistance := math.MaxFloat64

	for _, cluster := range lcm.clusters {
		distance := lcm.calculateDistance(
			sample.Location.Latitude, sample.Location.Longitude,
			cluster.CenterLatitude, cluster.CenterLongitude,
		)

		if distance <= lcm.config.ClusterMergeDistance && distance < minDistance {
			closestCluster = cluster
			minDistance = distance
		}
	}

	if closestCluster != nil {
		return closestCluster
	}

	// Create new cluster if we haven't reached the maximum
	if len(lcm.clusters) >= lcm.config.MaxClusters {
		// Remove oldest cluster
		lcm.removeOldestCluster()
	}

	newCluster := &pkg.LocationCluster{
		CenterLatitude:  sample.Location.Latitude,
		CenterLongitude: sample.Location.Longitude,
		Radius:          lcm.config.MinClusterRadius,
		SampleCount:     0,
		AvgLatency:      0,
		AvgLoss:         0,
		AvgObstruction:  0,
		Problematic:     false,
		FirstSeen:       time.Now(),
		LastSeen:        time.Now(),
	}

	lcm.clusters = append(lcm.clusters, newCluster)

	lcm.logger.LogStateChange("location_clustering", "no_cluster", "new_cluster", "cluster_created", map[string]interface{}{
		"center_lat":     newCluster.CenterLatitude,
		"center_lon":     newCluster.CenterLongitude,
		"total_clusters": len(lcm.clusters),
	})

	return newCluster
}

// updateClusterStatistics updates cluster statistics with new sample
func (lcm *LocationClusterManager) updateClusterStatistics(cluster *pkg.LocationCluster, sample *PerformanceSample) {
	// Update cluster center using weighted average
	weight := 1.0 / float64(cluster.SampleCount+1)
	cluster.CenterLatitude = cluster.CenterLatitude*(1-weight) + sample.Location.Latitude*weight
	cluster.CenterLongitude = cluster.CenterLongitude*(1-weight) + sample.Location.Longitude*weight

	// Update performance metrics using exponential moving average
	alpha := 0.1 // Smoothing factor
	if cluster.SampleCount == 0 {
		cluster.AvgLatency = sample.Latency
		cluster.AvgLoss = sample.Loss
		cluster.AvgObstruction = sample.Obstruction
	} else {
		cluster.AvgLatency = cluster.AvgLatency*(1-alpha) + sample.Latency*alpha
		cluster.AvgLoss = cluster.AvgLoss*(1-alpha) + sample.Loss*alpha
		cluster.AvgObstruction = cluster.AvgObstruction*(1-alpha) + sample.Obstruction*alpha
	}

	cluster.SampleCount++
	cluster.LastSeen = time.Now()

	// Update cluster radius based on sample distribution
	distance := lcm.calculateDistance(
		sample.Location.Latitude, sample.Location.Longitude,
		cluster.CenterLatitude, cluster.CenterLongitude,
	)

	if distance > cluster.Radius {
		cluster.Radius = math.Min(distance*1.1, lcm.config.MaxClusterRadius)
	}
}

// evaluateClusterHealth evaluates if a cluster should be marked as problematic
func (lcm *LocationClusterManager) evaluateClusterHealth(cluster *pkg.LocationCluster) {
	if cluster.SampleCount < lcm.config.MinSamplesPerCluster {
		return // Not enough samples yet
	}

	// Calculate performance score (0-100)
	latencyScore := math.Max(0, 100-cluster.AvgLatency/10)        // 10ms = 1 point penalty
	lossScore := math.Max(0, 100-cluster.AvgLoss*10)              // 1% loss = 10 points penalty
	obstructionScore := math.Max(0, 100-cluster.AvgObstruction*5) // 1% obstruction = 5 points penalty

	performanceScore := (latencyScore + lossScore + obstructionScore) / 3

	wasProblematic := cluster.Problematic
	cluster.Problematic = performanceScore < lcm.config.ProblematicThreshold

	if cluster.Problematic != wasProblematic {
		lcm.logger.LogStateChange("location_clustering",
			map[bool]string{true: "problematic", false: "healthy"}[wasProblematic],
			map[bool]string{true: "problematic", false: "healthy"}[cluster.Problematic],
			"cluster_health_changed", map[string]interface{}{
				"center_lat":        cluster.CenterLatitude,
				"center_lon":        cluster.CenterLongitude,
				"performance_score": performanceScore,
				"avg_latency":       cluster.AvgLatency,
				"avg_loss":          cluster.AvgLoss,
				"avg_obstruction":   cluster.AvgObstruction,
				"sample_count":      cluster.SampleCount,
			})
	}
}

// GetProblematicAreas returns clusters identified as problematic
func (lcm *LocationClusterManager) GetProblematicAreas() []*pkg.LocationCluster {
	var problematic []*pkg.LocationCluster

	for _, cluster := range lcm.clusters {
		if cluster.Problematic && cluster.SampleCount >= lcm.config.MinSamplesPerCluster {
			problematic = append(problematic, cluster)
		}
	}

	// Sort by sample count (most problematic first)
	sort.Slice(problematic, func(i, j int) bool {
		return problematic[i].SampleCount > problematic[j].SampleCount
	})

	return problematic
}

// IsLocationProblematic checks if a given location is in a problematic area
func (lcm *LocationClusterManager) IsLocationProblematic(location *pkg.GPSData) (bool, *pkg.LocationCluster) {
	if location == nil || !location.Valid {
		return false, nil
	}

	for _, cluster := range lcm.clusters {
		if !cluster.Problematic {
			continue
		}

		distance := lcm.calculateDistance(
			location.Latitude, location.Longitude,
			cluster.CenterLatitude, cluster.CenterLongitude,
		)

		if distance <= cluster.Radius {
			return true, cluster
		}
	}

	return false, nil
}

// GetLocationBasedThresholds returns adjusted thresholds based on location
func (lcm *LocationClusterManager) GetLocationBasedThresholds(location *pkg.GPSData) map[string]float64 {
	defaults := map[string]float64{
		"latency_threshold":     200.0, // ms
		"loss_threshold":        2.0,   // %
		"obstruction_threshold": 5.0,   // %
	}

	if location == nil || !location.Valid {
		return defaults
	}

	// Find the closest cluster
	var closestCluster *pkg.LocationCluster
	minDistance := math.MaxFloat64

	for _, cluster := range lcm.clusters {
		if cluster.SampleCount < lcm.config.MinSamplesPerCluster {
			continue
		}

		distance := lcm.calculateDistance(
			location.Latitude, location.Longitude,
			cluster.CenterLatitude, cluster.CenterLongitude,
		)

		if distance <= cluster.Radius && distance < minDistance {
			closestCluster = cluster
			minDistance = distance
		}
	}

	if closestCluster == nil {
		return defaults
	}

	// Adjust thresholds based on cluster performance
	adjustedThresholds := make(map[string]float64)

	if closestCluster.Problematic {
		// Relax thresholds for problematic areas
		adjustedThresholds["latency_threshold"] = defaults["latency_threshold"] * 1.5
		adjustedThresholds["loss_threshold"] = defaults["loss_threshold"] * 2.0
		adjustedThresholds["obstruction_threshold"] = defaults["obstruction_threshold"] * 2.0
	} else {
		// Use stricter thresholds for good areas
		adjustedThresholds["latency_threshold"] = defaults["latency_threshold"] * 0.8
		adjustedThresholds["loss_threshold"] = defaults["loss_threshold"] * 0.5
		adjustedThresholds["obstruction_threshold"] = defaults["obstruction_threshold"] * 0.7
	}

	lcm.logger.LogVerbose("location_based_thresholds", map[string]interface{}{
		"location_lat":         location.Latitude,
		"location_lon":         location.Longitude,
		"cluster_problematic":  closestCluster.Problematic,
		"cluster_sample_count": closestCluster.SampleCount,
		"adjusted_thresholds":  adjustedThresholds,
	})

	return adjustedThresholds
}

// performMaintenance performs cluster maintenance tasks
func (lcm *LocationClusterManager) performMaintenance() {
	// Remove expired clusters
	cutoff := time.Now().AddDate(0, 0, -lcm.config.ClusterExpiryDays)
	var activeClusters []*pkg.LocationCluster

	for _, cluster := range lcm.clusters {
		if cluster.LastSeen.After(cutoff) {
			activeClusters = append(activeClusters, cluster)
		}
	}

	if len(activeClusters) != len(lcm.clusters) {
		removed := len(lcm.clusters) - len(activeClusters)
		lcm.clusters = activeClusters

		lcm.logger.LogVerbose("cluster_maintenance", map[string]interface{}{
			"expired_clusters_removed": removed,
			"active_clusters":          len(activeClusters),
		})
	}

	// Merge nearby clusters if needed
	lcm.mergeNearbyClusters()
}

// mergeNearbyClusters merges clusters that are too close together
func (lcm *LocationClusterManager) mergeNearbyClusters() {
	for i := 0; i < len(lcm.clusters); i++ {
		for j := i + 1; j < len(lcm.clusters); j++ {
			cluster1 := lcm.clusters[i]
			cluster2 := lcm.clusters[j]

			distance := lcm.calculateDistance(
				cluster1.CenterLatitude, cluster1.CenterLongitude,
				cluster2.CenterLatitude, cluster2.CenterLongitude,
			)

			if distance <= lcm.config.ClusterMergeDistance {
				// Merge cluster2 into cluster1
				lcm.mergeClusters(cluster1, cluster2)

				// Remove cluster2
				lcm.clusters = append(lcm.clusters[:j], lcm.clusters[j+1:]...)
				j-- // Adjust index after removal

				lcm.logger.LogVerbose("clusters_merged", map[string]interface{}{
					"distance":           distance,
					"merge_distance":     lcm.config.ClusterMergeDistance,
					"remaining_clusters": len(lcm.clusters),
				})
			}
		}
	}
}

// mergeClusters merges two clusters
func (lcm *LocationClusterManager) mergeClusters(target, source *pkg.LocationCluster) {
	totalSamples := target.SampleCount + source.SampleCount
	weight1 := float64(target.SampleCount) / float64(totalSamples)
	weight2 := float64(source.SampleCount) / float64(totalSamples)

	// Merge center coordinates
	target.CenterLatitude = target.CenterLatitude*weight1 + source.CenterLatitude*weight2
	target.CenterLongitude = target.CenterLongitude*weight1 + source.CenterLongitude*weight2

	// Merge performance metrics
	target.AvgLatency = target.AvgLatency*weight1 + source.AvgLatency*weight2
	target.AvgLoss = target.AvgLoss*weight1 + source.AvgLoss*weight2
	target.AvgObstruction = target.AvgObstruction*weight1 + source.AvgObstruction*weight2

	// Update other properties
	target.SampleCount = totalSamples
	target.Radius = math.Max(target.Radius, source.Radius)
	target.Problematic = target.Problematic || source.Problematic

	if source.FirstSeen.Before(target.FirstSeen) {
		target.FirstSeen = source.FirstSeen
	}
	if source.LastSeen.After(target.LastSeen) {
		target.LastSeen = source.LastSeen
	}
}

// removeOldestCluster removes the cluster with the oldest last seen time
func (lcm *LocationClusterManager) removeOldestCluster() {
	if len(lcm.clusters) == 0 {
		return
	}

	oldestIndex := 0
	oldestTime := lcm.clusters[0].LastSeen

	for i, cluster := range lcm.clusters {
		if cluster.LastSeen.Before(oldestTime) {
			oldestIndex = i
			oldestTime = cluster.LastSeen
		}
	}

	lcm.clusters = append(lcm.clusters[:oldestIndex], lcm.clusters[oldestIndex+1:]...)
}

// calculateDistance calculates distance between two coordinates using Haversine formula
func (lcm *LocationClusterManager) calculateDistance(lat1, lon1, lat2, lon2 float64) float64 {
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

// GetClusterStatistics returns statistics about all clusters
func (lcm *LocationClusterManager) GetClusterStatistics() map[string]interface{} {
	totalClusters := len(lcm.clusters)
	problematicClusters := 0
	totalSamples := 0

	for _, cluster := range lcm.clusters {
		if cluster.Problematic {
			problematicClusters++
		}
		totalSamples += cluster.SampleCount
	}

	return map[string]interface{}{
		"total_clusters":       totalClusters,
		"problematic_clusters": problematicClusters,
		"healthy_clusters":     totalClusters - problematicClusters,
		"total_samples":        totalSamples,
		"avg_samples_per_cluster": func() float64 {
			if totalClusters > 0 {
				return float64(totalSamples) / float64(totalClusters)
			}
			return 0
		}(),
	}
}

// ExportClusters returns all clusters for analysis or storage
func (lcm *LocationClusterManager) ExportClusters() []*pkg.LocationCluster {
	// Return a copy to prevent external modification
	exported := make([]*pkg.LocationCluster, len(lcm.clusters))
	copy(exported, lcm.clusters)
	return exported
}
