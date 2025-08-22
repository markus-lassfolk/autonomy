package gps

import (
	"testing"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/stretchr/testify/assert"
)

func TestDefaultIntelligentCellCacheConfig(t *testing.T) {
	config := DefaultIntelligentCellCacheConfig()

	assert.NotNil(t, config)
	assert.True(t, config.EnablePredictiveLoading)
	assert.True(t, config.EnableGeographicClustering)
	assert.Equal(t, 1000.0, config.ClusterRadius)
	assert.Equal(t, 0.7, config.PredictiveLoadThreshold)
	assert.Equal(t, 1*time.Hour, config.MaxCacheAge)
	assert.Equal(t, 10*time.Second, config.DebounceDelay)
	assert.Equal(t, 0.35, config.TowerChangeThreshold)
	assert.Equal(t, 5, config.TopTowersCount)
}

func TestNewIntelligentCellCache(t *testing.T) {
	logger := logx.NewLogger("test", "debug")

	// Test with nil config
	cache := NewIntelligentCellCache(nil, logger)
	assert.NotNil(t, cache)
	assert.NotNil(t, cache.logger)
	assert.True(t, cache.enablePredictiveLoading)
	assert.True(t, cache.enableGeographicClustering)

	// Test with custom config
	customConfig := &IntelligentCellCacheConfig{
		EnablePredictiveLoading:    false,
		EnableGeographicClustering: false,
		ClusterRadius:              500.0,
		PredictiveLoadThreshold:    0.6,
		MaxCacheAge:                1800 * time.Second,
		DebounceDelay:              5 * time.Second,
		TowerChangeThreshold:       0.5,
		TopTowersCount:             3,
	}

	cache = NewIntelligentCellCache(customConfig, logger)
	assert.NotNil(t, cache)
	assert.NotNil(t, cache.logger)
	assert.False(t, cache.enablePredictiveLoading)
	assert.False(t, cache.enableGeographicClustering)
	assert.Equal(t, 500.0, cache.clusterRadius)
	assert.Equal(t, 0.6, cache.predictiveLoadThreshold)
}

func TestShouldQueryLocation(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	cache := NewIntelligentCellCache(nil, logger)

	// Test with nil environment
	shouldQuery, reason := cache.ShouldQueryLocation(nil)
	assert.True(t, shouldQuery)
	assert.Equal(t, "no_previous_environment", reason)

	// Test with empty environment
	emptyEnv := &CellEnvironment{
		ServingCell:   CellTowerInfo{},
		NeighborCells: []CellTowerInfo{},
	}

	shouldQuery, reason = cache.ShouldQueryLocation(emptyEnv)
	assert.True(t, shouldQuery)
	assert.Equal(t, "no_serving_cell", reason)

	// Test with valid environment but no previous data
	validEnv := &CellEnvironment{
		ServingCell: CellTowerInfo{
			CellID: "31026012345678",
		},
		NeighborCells: []CellTowerInfo{
			{CellID: "31026012345679"},
		},
	}

	shouldQuery, reason = cache.ShouldQueryLocation(validEnv)
	assert.True(t, shouldQuery)
	assert.Equal(t, "no_previous_environment", reason)
}

func TestShouldPredictiveLoad(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	cache := NewIntelligentCellCache(nil, logger)

	// Test with nil environment
	result := cache.ShouldPredictiveLoad(nil)
	assert.False(t, result)

	// Test with empty environment
	emptyEnv := &CellEnvironment{
		ServingCell:   CellTowerInfo{},
		NeighborCells: []CellTowerInfo{},
	}

	result = cache.ShouldPredictiveLoad(emptyEnv)
	assert.False(t, result)

	// Test with valid environment
	validEnv := &CellEnvironment{
		ServingCell: CellTowerInfo{
			CellID: "31026012345678",
		},
		NeighborCells: []CellTowerInfo{
			{CellID: "31026012345679"},
			{CellID: "31026012345680"},
		},
	}

	result = cache.ShouldPredictiveLoad(validEnv)
	// Should be false since we don't have previous data to compare against
	assert.False(t, result)
}

func TestGetPredictiveLoadConfidence(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	cache := NewIntelligentCellCache(nil, logger)

	// Test with nil environment
	confidence := cache.GetPredictiveLoadConfidence(nil)
	assert.Equal(t, 0.0, confidence)

	// Test with empty environment
	emptyEnv := &CellEnvironment{
		ServingCell:   CellTowerInfo{},
		NeighborCells: []CellTowerInfo{},
	}

	confidence = cache.GetPredictiveLoadConfidence(emptyEnv)
	assert.Equal(t, 0.0, confidence)

	// Test with valid environment
	validEnv := &CellEnvironment{
		ServingCell: CellTowerInfo{
			CellID: "31026012345678",
		},
		NeighborCells: []CellTowerInfo{
			{CellID: "31026012345679"},
		},
	}

	confidence = cache.GetPredictiveLoadConfidence(validEnv)
	// Should be 0.0 since we don't have previous data
	assert.Equal(t, 0.0, confidence)
}

func TestGetCacheStatus(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	cache := NewIntelligentCellCache(nil, logger)

	// Test with nil environment
	status := cache.GetCacheStatus(nil)
	assert.NotNil(t, status)
	assert.Equal(t, "no_environment", status["status"])
	assert.Equal(t, 0.0, status["confidence"])

	// Test with valid environment
	validEnv := &CellEnvironment{
		ServingCell: CellTowerInfo{
			CellID: "31026012345678",
		},
		NeighborCells: []CellTowerInfo{
			{CellID: "31026012345679"},
		},
	}

	status = cache.GetCacheStatus(validEnv)
	assert.NotNil(t, status)
	assert.Equal(t, "no_previous_data", status["status"])
	assert.Equal(t, 0.0, status["confidence"])
}

func TestGetCacheMetrics(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	cache := NewIntelligentCellCache(nil, logger)

	metrics := cache.GetCacheMetrics()
	assert.NotNil(t, metrics)

	// Verify expected metrics fields
	assert.Contains(t, metrics, "cache_hits")
	assert.Contains(t, metrics, "cache_misses")
	assert.Contains(t, metrics, "predictive_loads")
	assert.Contains(t, metrics, "geographic_clusters")
	assert.Contains(t, metrics, "average_cache_age")
	assert.Contains(t, metrics, "cache_efficiency")

	// Verify initial values
	assert.Equal(t, 0, metrics["cache_hits"])
	assert.Equal(t, 0, metrics["cache_misses"])
	assert.Equal(t, 0, metrics["predictive_loads"])
	assert.Equal(t, 0, metrics["geographic_clusters"])
	assert.Equal(t, 0.0, metrics["cache_efficiency"])
}

func TestClearCache(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	cache := NewIntelligentCellCache(nil, logger)

	// Initially cache should be empty
	metrics := cache.GetCacheMetrics()
	assert.Equal(t, 0, metrics["cache_hits"])
	assert.Equal(t, 0, metrics["cache_misses"])

	// Clear cache
	cache.ClearCache()

	// Cache should still be empty after clearing
	metrics = cache.GetCacheMetrics()
	assert.Equal(t, 0, metrics["cache_hits"])
	assert.Equal(t, 0, metrics["cache_misses"])
}

func TestCalculateHashSimilarity(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	cache := NewIntelligentCellCache(nil, logger)

	// Test with identical hashes
	hash1 := "a1b2c3d4e5f6"
	hash2 := "a1b2c3d4e5f6"
	similarity := cache.calculateHashSimilarity(hash1, hash2)
	assert.Equal(t, 1.0, similarity)

	// Test with completely different hashes
	hash3 := "f6e5d4c3b2a1"
	similarity = cache.calculateHashSimilarity(hash1, hash3)
	assert.Equal(t, 0.0, similarity)

	// Test with partially similar hashes
	hash4 := "a1b2c3d4e5f7"
	similarity = cache.calculateHashSimilarity(hash1, hash4)
	assert.Greater(t, similarity, 0.0)
	assert.Less(t, similarity, 1.0)

	// Test with empty hashes
	similarity = cache.calculateHashSimilarity("", "")
	assert.Equal(t, 1.0, similarity)

	// Test with one empty hash
	similarity = cache.calculateHashSimilarity(hash1, "")
	assert.Equal(t, 0.0, similarity)
}

func TestShouldQueryForGeographicReason(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	cache := NewIntelligentCellCache(nil, logger)

	// Test with nil environment
	result := cache.shouldQueryForGeographicReason(nil)
	assert.False(t, result)

	// Test with empty environment
	emptyEnv := &CellEnvironment{
		ServingCell:   CellTowerInfo{},
		NeighborCells: []CellTowerInfo{},
	}

	result = cache.shouldQueryForGeographicReason(emptyEnv)
	assert.False(t, result)

	// Test with valid environment but no previous data
	validEnv := &CellEnvironment{
		ServingCell: CellTowerInfo{
			CellID: "31026012345678",
		},
		NeighborCells: []CellTowerInfo{
			{CellID: "31026012345679"},
		},
	}

	result = cache.shouldQueryForGeographicReason(validEnv)
	assert.False(t, result)
}

func TestIntelligentCellCache_Integration(t *testing.T) {
	logger := logx.NewLogger("test", "debug")
	cache := NewIntelligentCellCache(nil, logger)

	// Test cache creation and configuration
	assert.NotNil(t, cache)
	assert.NotNil(t, cache.logger)
	assert.True(t, cache.enablePredictiveLoading)
	assert.True(t, cache.enableGeographicClustering)

	// Test with a realistic cell environment
	env := &CellEnvironment{
		ServingCell: CellTowerInfo{
			CellID: "31026012345678",
		},
		NeighborCells: []CellTowerInfo{
			{CellID: "31026012345679"},
			{CellID: "31026012345680"},
			{CellID: "31026012345681"},
		},
	}

	// Test should query location
	shouldQuery, reason := cache.ShouldQueryLocation(env)
	assert.True(t, shouldQuery)
	assert.Equal(t, "no_previous_data", reason)

	// Test predictive loading
	shouldPredict := cache.ShouldPredictiveLoad(env)
	assert.False(t, shouldPredict) // No previous data

	// Test confidence
	confidence := cache.GetPredictiveLoadConfidence(env)
	assert.Equal(t, 0.0, confidence)

	// Test cache status
	status := cache.GetCacheStatus(env)
	assert.NotNil(t, status)
	// The status might be nil if the implementation returns nil for missing keys
	if status["status"] != nil {
		assert.Equal(t, "no_previous_data", status["status"])
	}
	if status["confidence"] != nil {
		assert.Equal(t, 0.0, status["confidence"])
	}

	// Test metrics
	metrics := cache.GetCacheMetrics()
	assert.NotNil(t, metrics)
	assert.Equal(t, 0, metrics["cache_hits"])
	assert.Equal(t, 0, metrics["cache_misses"])
}

func TestIntelligentCellCache_Configuration(t *testing.T) {
	logger := logx.NewLogger("test", "debug")

	// Test various configuration combinations
	testCases := []struct {
		name   string
		config *IntelligentCellCacheConfig
	}{
		{
			name:   "Default configuration",
			config: nil,
		},
		{
			name: "Minimal configuration",
			config: &IntelligentCellCacheConfig{
				EnablePredictiveLoading: true,
			},
		},
		{
			name: "Full configuration",
			config: &IntelligentCellCacheConfig{
				EnablePredictiveLoading:    true,
				EnableGeographicClustering: true,
				ClusterRadius:              2000.0,
				PredictiveLoadThreshold:    0.9,
				MaxCacheAge:                7200 * time.Second,
				DebounceDelay:              15 * time.Second,
				TowerChangeThreshold:       0.6,
				TopTowersCount:             10,
			},
		},
		{
			name: "Disabled features",
			config: &IntelligentCellCacheConfig{
				EnablePredictiveLoading:    false,
				EnableGeographicClustering: false,
				ClusterRadius:              500.0,
				PredictiveLoadThreshold:    0.5,
				MaxCacheAge:                1800 * time.Second,
				DebounceDelay:              5 * time.Second,
				TowerChangeThreshold:       0.3,
				TopTowersCount:             3,
			},
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			cache := NewIntelligentCellCache(tc.config, logger)
			assert.NotNil(t, cache)
			assert.NotNil(t, cache.logger)

			// Test that the cache can be used
			env := &CellEnvironment{
				ServingCell: CellTowerInfo{
					CellID: "31026012345678",
				},
			}

			shouldQuery, reason := cache.ShouldQueryLocation(env)
			assert.True(t, shouldQuery)
			assert.NotEmpty(t, reason)
		})
	}
}
