package ubus

import (
	"context"
	"encoding/json"
	"fmt"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/location"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// LocationHandlers provides ubus API endpoints for location clustering
type LocationHandlers struct {
	clusterManager *location.ClusterManager
	logger         *logx.Logger
}

// NewLocationHandlers creates a new location handlers instance
func NewLocationHandlers(clusterManager *location.ClusterManager, logger *logx.Logger) *LocationHandlers {
	return &LocationHandlers{
		clusterManager: clusterManager,
		logger:         logger,
	}
}

// GetClusters returns all location clusters
func (lh *LocationHandlers) GetClusters(ctx context.Context) (map[string]interface{}, error) {
	clusters := lh.clusterManager.GetClusters()

	// Convert clusters to JSON-serializable format
	clusterData := make([]map[string]interface{}, 0, len(clusters))
	for _, cluster := range clusters {
		clusterData = append(clusterData, map[string]interface{}{
			"id": cluster.ID,
			"center": map[string]interface{}{
				"latitude":  cluster.Center.Latitude,
				"longitude": cluster.Center.Longitude,
			},
			"radius":       cluster.Radius,
			"points_count": len(cluster.Points),
			"created_at":   cluster.CreatedAt.Format(time.RFC3339),
			"last_updated": cluster.LastUpdated.Format(time.RFC3339),
			"visit_count":  cluster.VisitCount,
			"total_time":   cluster.TotalTime,
			"avg_signal":   cluster.AvgSignal,
			"avg_speed":    cluster.AvgSpeed,
			"tags":         cluster.Tags,
		})
	}

	return map[string]interface{}{
		"clusters": clusterData,
		"count":    len(clusters),
	}, nil
}

// GetClusterByID returns a specific cluster by ID
func (lh *LocationHandlers) GetClusterByID(ctx context.Context, id string) (map[string]interface{}, error) {
	cluster, exists := lh.clusterManager.GetClusterByID(id)
	if !exists {
		return nil, fmt.Errorf("cluster not found: %s", id)
	}

	// Convert points to JSON-serializable format
	points := make([]map[string]interface{}, 0, len(cluster.Points))
	for _, point := range cluster.Points {
		points = append(points, map[string]interface{}{
			"latitude":    point.Latitude,
			"longitude":   point.Longitude,
			"timestamp":   point.Timestamp.Format(time.RFC3339),
			"signal":      point.Signal,
			"speed":       point.Speed,
			"member":      point.Member,
			"obstruction": point.Obstruction,
			"tags":        point.Tags,
		})
	}

	return map[string]interface{}{
		"id": cluster.ID,
		"center": map[string]interface{}{
			"latitude":  cluster.Center.Latitude,
			"longitude": cluster.Center.Longitude,
		},
		"radius":       cluster.Radius,
		"points":       points,
		"created_at":   cluster.CreatedAt.Format(time.RFC3339),
		"last_updated": cluster.LastUpdated.Format(time.RFC3339),
		"visit_count":  cluster.VisitCount,
		"total_time":   cluster.TotalTime,
		"avg_signal":   cluster.AvgSignal,
		"avg_speed":    cluster.AvgSpeed,
		"tags":         cluster.Tags,
	}, nil
}

// AddPoint adds a new point to the clustering system
func (lh *LocationHandlers) AddPoint(ctx context.Context, data map[string]interface{}) (map[string]interface{}, error) {
	// Parse point data
	latitude, ok := data["latitude"].(float64)
	if !ok {
		return nil, fmt.Errorf("latitude is required and must be a number")
	}

	longitude, ok := data["longitude"].(float64)
	if !ok {
		return nil, fmt.Errorf("longitude is required and must be a number")
	}

	// Parse timestamp (default to now if not provided)
	var timestamp time.Time
	if tsStr, ok := data["timestamp"].(string); ok {
		var err error
		timestamp, err = time.Parse(time.RFC3339, tsStr)
		if err != nil {
			return nil, fmt.Errorf("invalid timestamp format: %v", err)
		}
	} else {
		timestamp = time.Now()
	}

	// Parse optional fields
	signal, _ := data["signal"].(float64)
	speed, _ := data["speed"].(float64)
	member, _ := data["member"].(string)
	obstruction, _ := data["obstruction"].(bool)

	// Parse tags
	var tags []string
	if tagsData, ok := data["tags"].([]interface{}); ok {
		for _, tag := range tagsData {
			if tagStr, ok := tag.(string); ok {
				tags = append(tags, tagStr)
			}
		}
	}

	point := location.Point{
		Latitude:    latitude,
		Longitude:   longitude,
		Timestamp:   timestamp,
		Signal:      signal,
		Speed:       speed,
		Member:      member,
		Obstruction: obstruction,
		Tags:        tags,
	}

	err := lh.clusterManager.AddPoint(point)
	if err != nil {
		return nil, fmt.Errorf("failed to add point: %v", err)
	}

	return map[string]interface{}{
		"success": true,
		"message": "Point added successfully",
	}, nil
}

// FindNearestCluster finds the nearest cluster to a given point
func (lh *LocationHandlers) FindNearestCluster(ctx context.Context, data map[string]interface{}) (map[string]interface{}, error) {
	latitude, ok := data["latitude"].(float64)
	if !ok {
		return nil, fmt.Errorf("latitude is required and must be a number")
	}

	longitude, ok := data["longitude"].(float64)
	if !ok {
		return nil, fmt.Errorf("longitude is required and must be a number")
	}

	point := location.Point{
		Latitude:  latitude,
		Longitude: longitude,
	}

	cluster, distance := lh.clusterManager.FindNearestCluster(point)
	if cluster == nil {
		return map[string]interface{}{
			"cluster":  nil,
			"distance": -1,
		}, nil
	}

	return map[string]interface{}{
		"cluster": map[string]interface{}{
			"id": cluster.ID,
			"center": map[string]interface{}{
				"latitude":  cluster.Center.Latitude,
				"longitude": cluster.Center.Longitude,
			},
			"radius":      cluster.Radius,
			"visit_count": cluster.VisitCount,
			"avg_signal":  cluster.AvgSignal,
			"avg_speed":   cluster.AvgSpeed,
			"tags":        cluster.Tags,
		},
		"distance": distance,
	}, nil
}

// GetClustersByTag returns clusters that have a specific tag
func (lh *LocationHandlers) GetClustersByTag(ctx context.Context, tag string) (map[string]interface{}, error) {
	clusters := lh.clusterManager.GetClustersByTag(tag)

	clusterData := make([]map[string]interface{}, 0, len(clusters))
	for _, cluster := range clusters {
		clusterData = append(clusterData, map[string]interface{}{
			"id": cluster.ID,
			"center": map[string]interface{}{
				"latitude":  cluster.Center.Latitude,
				"longitude": cluster.Center.Longitude,
			},
			"radius":       cluster.Radius,
			"points_count": len(cluster.Points),
			"visit_count":  cluster.VisitCount,
			"avg_signal":   cluster.AvgSignal,
			"avg_speed":    cluster.AvgSpeed,
			"tags":         cluster.Tags,
		})
	}

	return map[string]interface{}{
		"clusters": clusterData,
		"tag":      tag,
		"count":    len(clusters),
	}, nil
}

// AddTagToCluster adds a tag to a specific cluster
func (lh *LocationHandlers) AddTagToCluster(ctx context.Context, data map[string]interface{}) (map[string]interface{}, error) {
	clusterID, ok := data["cluster_id"].(string)
	if !ok {
		return nil, fmt.Errorf("cluster_id is required")
	}

	tag, ok := data["tag"].(string)
	if !ok {
		return nil, fmt.Errorf("tag is required")
	}

	err := lh.clusterManager.AddTagToCluster(clusterID, tag)
	if err != nil {
		return nil, fmt.Errorf("failed to add tag: %v", err)
	}

	return map[string]interface{}{
		"success": true,
		"message": "Tag added successfully",
	}, nil
}

// RemoveTagFromCluster removes a tag from a specific cluster
func (lh *LocationHandlers) RemoveTagFromCluster(ctx context.Context, data map[string]interface{}) (map[string]interface{}, error) {
	clusterID, ok := data["cluster_id"].(string)
	if !ok {
		return nil, fmt.Errorf("cluster_id is required")
	}

	tag, ok := data["tag"].(string)
	if !ok {
		return nil, fmt.Errorf("tag is required")
	}

	err := lh.clusterManager.RemoveTagFromCluster(clusterID, tag)
	if err != nil {
		return nil, fmt.Errorf("failed to remove tag: %v", err)
	}

	return map[string]interface{}{
		"success": true,
		"message": "Tag removed successfully",
	}, nil
}

// GetStatistics returns clustering statistics
func (lh *LocationHandlers) GetStatistics(ctx context.Context) (map[string]interface{}, error) {
	stats := lh.clusterManager.GetStatistics()
	return stats, nil
}

// UpdateConfig updates the clustering configuration
func (lh *LocationHandlers) UpdateConfig(ctx context.Context, data map[string]interface{}) (map[string]interface{}, error) {
	configData, err := json.Marshal(data)
	if err != nil {
		return nil, fmt.Errorf("failed to marshal config data: %v", err)
	}

	var config location.ClusteringConfig
	err = json.Unmarshal(configData, &config)
	if err != nil {
		return nil, fmt.Errorf("failed to unmarshal config: %v", err)
	}

	lh.clusterManager.UpdateConfig(&config)

	return map[string]interface{}{
		"success": true,
		"message": "Configuration updated successfully",
	}, nil
}

// GetConfig returns the current clustering configuration
func (lh *LocationHandlers) GetConfig(ctx context.Context) (map[string]interface{}, error) {
	// This would require exposing the config from ClusterManager
	// For now, return a default config
	return map[string]interface{}{
		"max_distance":     100.0,
		"min_points":       3,
		"max_radius":       500.0,
		"time_window":      3600.0,
		"merge_threshold":  200.0,
		"cleanup_interval": 86400.0,
		"max_clusters":     100,
	}, nil
}
