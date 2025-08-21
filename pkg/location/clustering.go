package location

import (
	"context"
	"fmt"
	"math"
	"sort"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// Cluster represents a group of nearby locations
type Cluster struct {
	ID          string    `json:"id"`
	Center      Point     `json:"center"`
	Radius      float64   `json:"radius"` // in meters
	Points      []Point   `json:"points"`
	CreatedAt   time.Time `json:"created_at"`
	LastUpdated time.Time `json:"last_updated"`
	VisitCount  int       `json:"visit_count"`
	TotalTime   float64   `json:"total_time"` // in seconds
	AvgSignal   float64   `json:"avg_signal"`
	AvgSpeed    float64   `json:"avg_speed"`
	Tags        []string  `json:"tags"`
}

// Point represents a GPS coordinate with metadata
type Point struct {
	Latitude    float64   `json:"latitude"`
	Longitude   float64   `json:"longitude"`
	Timestamp   time.Time `json:"timestamp"`
	Signal      float64   `json:"signal"`
	Speed       float64   `json:"speed"`
	Member      string    `json:"member"`
	Obstruction bool      `json:"obstruction"`
	Tags        []string  `json:"tags"`
}

// ClusteringConfig holds configuration for the clustering algorithm
type ClusteringConfig struct {
	MaxDistance     float64 `json:"max_distance"`     // Maximum distance between points in a cluster (meters)
	MinPoints       int     `json:"min_points"`       // Minimum points required to form a cluster
	MaxRadius       float64 `json:"max_radius"`       // Maximum radius for a cluster (meters)
	TimeWindow      float64 `json:"time_window"`      // Time window for clustering (seconds)
	MergeThreshold  float64 `json:"merge_threshold"`  // Distance threshold for merging clusters
	CleanupInterval float64 `json:"cleanup_interval"` // Cleanup interval (seconds)
	MaxClusters     int     `json:"max_clusters"`     // Maximum number of clusters to maintain
}

// ClusterManager manages location clustering
type ClusterManager struct {
	config   *ClusteringConfig
	clusters map[string]*Cluster
	points   []Point
	mu       sync.RWMutex
	logger   *logx.Logger
	ctx      context.Context
	cancel   context.CancelFunc
}

// NewClusterManager creates a new cluster manager
func NewClusterManager(config *ClusteringConfig, logger *logx.Logger) *ClusterManager {
	if config == nil {
		config = &ClusteringConfig{
			MaxDistance:     100.0,   // 100 meters
			MinPoints:       3,       // 3 points minimum
			MaxRadius:       500.0,   // 500 meters max radius
			TimeWindow:      3600.0,  // 1 hour
			MergeThreshold:  200.0,   // 200 meters
			CleanupInterval: 86400.0, // 24 hours
			MaxClusters:     100,     // 100 clusters max
		}
	}

	ctx, cancel := context.WithCancel(context.Background())
	cm := &ClusterManager{
		config:   config,
		clusters: make(map[string]*Cluster),
		points:   make([]Point, 0),
		logger:   logger,
		ctx:      ctx,
		cancel:   cancel,
	}

	// Start cleanup routine
	go cm.cleanupRoutine()

	return cm
}

// AddPoint adds a new point to the clustering system
func (cm *ClusterManager) AddPoint(point Point) error {
	cm.mu.Lock()
	defer cm.mu.Unlock()

	// Add point to the list
	cm.points = append(cm.points, point)

	// Try to assign to existing cluster
	assigned := cm.assignToExistingCluster(point)
	if !assigned {
		// Create new cluster if possible
		cm.createNewCluster(point)
	}

	// Cleanup old points
	cm.cleanupOldPoints()

	// Merge nearby clusters
	cm.mergeNearbyClusters()

	// Limit total clusters
	cm.limitClusters()

	return nil
}

// assignToExistingCluster tries to assign a point to an existing cluster
func (cm *ClusterManager) assignToExistingCluster(point Point) bool {
	for _, cluster := range cm.clusters {
		distance := cm.calculateDistance(point, cluster.Center)
		if distance <= cm.config.MaxDistance {
			// Add point to cluster
			cluster.Points = append(cluster.Points, point)
			cluster.LastUpdated = time.Now()
			cluster.VisitCount++

			// Update cluster center and radius
			cm.updateClusterMetrics(cluster)

			cm.logger.Debug("Point assigned to existing cluster", "cluster_id", cluster.ID, "distance", distance)
			return true
		}
	}
	return false
}

// createNewCluster creates a new cluster from a point
func (cm *ClusterManager) createNewCluster(point Point) {
	// Check if we have enough nearby points to form a cluster
	nearbyPoints := cm.findNearbyPoints(point, cm.config.MaxDistance)
	if len(nearbyPoints) < cm.config.MinPoints {
		return
	}

	// Create new cluster
	clusterID := cm.generateClusterID()
	cluster := &Cluster{
		ID:          clusterID,
		Center:      point,
		Radius:      cm.config.MaxDistance,
		Points:      nearbyPoints,
		CreatedAt:   time.Now(),
		LastUpdated: time.Now(),
		VisitCount:  1,
		Tags:        make([]string, 0),
	}

	// Calculate cluster metrics
	cm.updateClusterMetrics(cluster)

	// Add cluster
	cm.clusters[clusterID] = cluster

	// Remove points from main list that are now in cluster
	cm.removePointsFromList(nearbyPoints)

	cm.logger.Info("Created new cluster", "cluster_id", clusterID, "points", len(nearbyPoints))
}

// findNearbyPoints finds points within a certain distance
func (cm *ClusterManager) findNearbyPoints(point Point, maxDistance float64) []Point {
	var nearby []Point
	for _, p := range cm.points {
		if cm.calculateDistance(point, p) <= maxDistance {
			nearby = append(nearby, p)
		}
	}
	return nearby
}

// updateClusterMetrics updates cluster center, radius, and other metrics
func (cm *ClusterManager) updateClusterMetrics(cluster *Cluster) {
	if len(cluster.Points) == 0 {
		return
	}

	// Calculate center
	var totalLat, totalLon float64
	var totalSignal, totalSpeed float64

	for _, point := range cluster.Points {
		totalLat += point.Latitude
		totalLon += point.Longitude
		totalSignal += point.Signal
		totalSpeed += point.Speed
	}

	cluster.Center.Latitude = totalLat / float64(len(cluster.Points))
	cluster.Center.Longitude = totalLon / float64(len(cluster.Points))
	cluster.AvgSignal = totalSignal / float64(len(cluster.Points))
	cluster.AvgSpeed = totalSpeed / float64(len(cluster.Points))

	// Calculate radius (distance from center to farthest point)
	maxDistance := 0.0
	for _, point := range cluster.Points {
		distance := cm.calculateDistance(point, cluster.Center)
		if distance > maxDistance {
			maxDistance = distance
		}
	}
	cluster.Radius = maxDistance

	// Calculate total time
	if len(cluster.Points) > 1 {
		sort.Slice(cluster.Points, func(i, j int) bool {
			return cluster.Points[i].Timestamp.Before(cluster.Points[j].Timestamp)
		})
		cluster.TotalTime = cluster.Points[len(cluster.Points)-1].Timestamp.Sub(cluster.Points[0].Timestamp).Seconds()
	}
}

// mergeNearbyClusters merges clusters that are close to each other
func (cm *ClusterManager) mergeNearbyClusters() {
	clusters := make([]*Cluster, 0, len(cm.clusters))
	for _, cluster := range cm.clusters {
		clusters = append(clusters, cluster)
	}

	for i := 0; i < len(clusters); i++ {
		for j := i + 1; j < len(clusters); j++ {
			distance := cm.calculateDistance(clusters[i].Center, clusters[j].Center)
			if distance <= cm.config.MergeThreshold {
				cm.mergeClusters(clusters[i], clusters[j])
				// Remove merged cluster
				delete(cm.clusters, clusters[j].ID)
				// Remove from slice
				clusters = append(clusters[:j], clusters[j+1:]...)
				j--
			}
		}
	}
}

// mergeClusters merges two clusters
func (cm *ClusterManager) mergeClusters(cluster1, cluster2 *Cluster) {
	// Merge points
	cluster1.Points = append(cluster1.Points, cluster2.Points...)

	// Update metrics
	cluster1.VisitCount += cluster2.VisitCount
	cluster1.TotalTime += cluster2.TotalTime
	cluster1.LastUpdated = time.Now()

	// Merge tags
	cluster1.Tags = cm.mergeTags(cluster1.Tags, cluster2.Tags)

	// Update cluster metrics
	cm.updateClusterMetrics(cluster1)

	cm.logger.Info("Merged clusters", "cluster1_id", cluster1.ID, "cluster2_id", cluster2.ID)
}

// mergeTags merges two tag slices without duplicates
func (cm *ClusterManager) mergeTags(tags1, tags2 []string) []string {
	tagMap := make(map[string]bool)

	for _, tag := range tags1 {
		tagMap[tag] = true
	}
	for _, tag := range tags2 {
		tagMap[tag] = true
	}

	result := make([]string, 0, len(tagMap))
	for tag := range tagMap {
		result = append(result, tag)
	}

	return result
}

// cleanupOldPoints removes points older than the time window
func (cm *ClusterManager) cleanupOldPoints() {
	cutoff := time.Now().Add(-time.Duration(cm.config.TimeWindow) * time.Second)

	// Clean main points list
	newPoints := make([]Point, 0)
	for _, point := range cm.points {
		if point.Timestamp.After(cutoff) {
			newPoints = append(newPoints, point)
		}
	}
	cm.points = newPoints

	// Clean cluster points
	for _, cluster := range cm.clusters {
		newClusterPoints := make([]Point, 0)
		for _, point := range cluster.Points {
			if point.Timestamp.After(cutoff) {
				newClusterPoints = append(newClusterPoints, point)
			}
		}
		cluster.Points = newClusterPoints

		// Update cluster metrics
		cm.updateClusterMetrics(cluster)
	}
}

// limitClusters limits the total number of clusters
func (cm *ClusterManager) limitClusters() {
	if len(cm.clusters) <= cm.config.MaxClusters {
		return
	}

	// Sort clusters by last updated time (oldest first)
	clusters := make([]*Cluster, 0, len(cm.clusters))
	for _, cluster := range cm.clusters {
		clusters = append(clusters, cluster)
	}

	sort.Slice(clusters, func(i, j int) bool {
		return clusters[i].LastUpdated.Before(clusters[j].LastUpdated)
	})

	// Remove oldest clusters
	toRemove := len(clusters) - cm.config.MaxClusters
	for i := 0; i < toRemove; i++ {
		delete(cm.clusters, clusters[i].ID)
		cm.logger.Debug("Removed old cluster", "cluster_id", clusters[i].ID)
	}
}

// removePointsFromList removes points from the main points list
func (cm *ClusterManager) removePointsFromList(pointsToRemove []Point) {
	// Create a map for fast lookup
	removeMap := make(map[string]bool)
	for _, point := range pointsToRemove {
		key := cm.pointKey(point)
		removeMap[key] = true
	}

	// Filter out points to remove
	newPoints := make([]Point, 0)
	for _, point := range cm.points {
		key := cm.pointKey(point)
		if !removeMap[key] {
			newPoints = append(newPoints, point)
		}
	}
	cm.points = newPoints
}

// pointKey generates a unique key for a point
func (cm *ClusterManager) pointKey(point Point) string {
	return fmt.Sprintf("%.6f,%.6f,%d", point.Latitude, point.Longitude, point.Timestamp.Unix())
}

// generateClusterID generates a unique cluster ID
func (cm *ClusterManager) generateClusterID() string {
	return fmt.Sprintf("cluster_%d", time.Now().UnixNano())
}

// calculateDistance calculates the distance between two points in meters
func (cm *ClusterManager) calculateDistance(p1, p2 Point) float64 {
	const earthRadius = 6371000 // Earth's radius in meters

	lat1 := p1.Latitude * math.Pi / 180
	lat2 := p2.Latitude * math.Pi / 180
	deltaLat := (p2.Latitude - p1.Latitude) * math.Pi / 180
	deltaLon := (p2.Longitude - p1.Longitude) * math.Pi / 180

	a := math.Sin(deltaLat/2)*math.Sin(deltaLat/2) +
		math.Cos(lat1)*math.Cos(lat2)*
			math.Sin(deltaLon/2)*math.Sin(deltaLon/2)
	c := 2 * math.Atan2(math.Sqrt(a), math.Sqrt(1-a))

	return earthRadius * c
}

// GetClusters returns all clusters
func (cm *ClusterManager) GetClusters() []*Cluster {
	cm.mu.RLock()
	defer cm.mu.RUnlock()

	clusters := make([]*Cluster, 0, len(cm.clusters))
	for _, cluster := range cm.clusters {
		clusters = append(clusters, cluster)
	}

	return clusters
}

// GetClusterByID returns a specific cluster by ID
func (cm *ClusterManager) GetClusterByID(id string) (*Cluster, bool) {
	cm.mu.RLock()
	defer cm.mu.RUnlock()

	cluster, exists := cm.clusters[id]
	return cluster, exists
}

// FindNearestCluster finds the nearest cluster to a given point
func (cm *ClusterManager) FindNearestCluster(point Point) (*Cluster, float64) {
	cm.mu.RLock()
	defer cm.mu.RUnlock()

	var nearestCluster *Cluster
	var minDistance float64 = math.MaxFloat64

	for _, cluster := range cm.clusters {
		distance := cm.calculateDistance(point, cluster.Center)
		if distance < minDistance {
			minDistance = distance
			nearestCluster = cluster
		}
	}

	return nearestCluster, minDistance
}

// GetClustersByTag returns clusters that have a specific tag
func (cm *ClusterManager) GetClustersByTag(tag string) []*Cluster {
	cm.mu.RLock()
	defer cm.mu.RUnlock()

	var result []*Cluster
	for _, cluster := range cm.clusters {
		for _, clusterTag := range cluster.Tags {
			if clusterTag == tag {
				result = append(result, cluster)
				break
			}
		}
	}

	return result
}

// AddTagToCluster adds a tag to a specific cluster
func (cm *ClusterManager) AddTagToCluster(clusterID, tag string) error {
	cm.mu.Lock()
	defer cm.mu.Unlock()

	cluster, exists := cm.clusters[clusterID]
	if !exists {
		return fmt.Errorf("cluster not found: %s", clusterID)
	}

	// Check if tag already exists
	for _, existingTag := range cluster.Tags {
		if existingTag == tag {
			return nil // Tag already exists
		}
	}

	cluster.Tags = append(cluster.Tags, tag)
	cluster.LastUpdated = time.Now()

	cm.logger.Debug("Added tag to cluster", "cluster_id", clusterID, "tag", tag)
	return nil
}

// RemoveTagFromCluster removes a tag from a specific cluster
func (cm *ClusterManager) RemoveTagFromCluster(clusterID, tag string) error {
	cm.mu.Lock()
	defer cm.mu.Unlock()

	cluster, exists := cm.clusters[clusterID]
	if !exists {
		return fmt.Errorf("cluster not found: %s", clusterID)
	}

	newTags := make([]string, 0)
	for _, existingTag := range cluster.Tags {
		if existingTag != tag {
			newTags = append(newTags, existingTag)
		}
	}

	cluster.Tags = newTags
	cluster.LastUpdated = time.Now()

	cm.logger.Debug("Removed tag from cluster", "cluster_id", clusterID, "tag", tag)
	return nil
}

// GetStatistics returns clustering statistics
func (cm *ClusterManager) GetStatistics() map[string]interface{} {
	cm.mu.RLock()
	defer cm.mu.RUnlock()

	totalPoints := len(cm.points)
	totalClusters := len(cm.clusters)

	var totalVisits int
	var avgRadius float64
	var avgPointsPerCluster float64

	if totalClusters > 0 {
		for _, cluster := range cm.clusters {
			totalVisits += cluster.VisitCount
			avgRadius += cluster.Radius
			avgPointsPerCluster += float64(len(cluster.Points))
		}
		avgRadius /= float64(totalClusters)
		avgPointsPerCluster /= float64(totalClusters)
	}

	return map[string]interface{}{
		"total_points":               totalPoints,
		"total_clusters":             totalClusters,
		"total_visits":               totalVisits,
		"average_radius":             avgRadius,
		"average_points_per_cluster": avgPointsPerCluster,
		"max_clusters":               cm.config.MaxClusters,
		"max_distance":               cm.config.MaxDistance,
		"min_points":                 cm.config.MinPoints,
	}
}

// cleanupRoutine runs periodic cleanup
func (cm *ClusterManager) cleanupRoutine() {
	ticker := time.NewTicker(time.Duration(cm.config.CleanupInterval) * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			cm.mu.Lock()
			cm.cleanupOldPoints()
			cm.mergeNearbyClusters()
			cm.limitClusters()
			cm.mu.Unlock()

			cm.logger.Debug("Performed periodic cluster cleanup")
		case <-cm.ctx.Done():
			return
		}
	}
}

// Close stops the cluster manager
func (cm *ClusterManager) Close() error {
	cm.cancel()
	return nil
}

// UpdateConfig updates the clustering configuration
func (cm *ClusterManager) UpdateConfig(config *ClusteringConfig) {
	cm.mu.Lock()
	defer cm.mu.Unlock()

	cm.config = config
	cm.logger.Info("Updated clustering configuration")
}
