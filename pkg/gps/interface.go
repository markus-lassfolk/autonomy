package gps

import (
	"context"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// GPSCollector interface for GPS data collection
type GPSCollector interface {
	// CollectGPS collects GPS data from the best available source
	CollectGPS(ctx context.Context) (*pkg.GPSData, error)

	// GetBestSource returns the name of the best available GPS source
	GetBestSource() string

	// ValidateGPS validates GPS data quality
	ValidateGPS(gps *pkg.GPSData) error
}

// ComprehensiveGPSCollectorInterface extends the basic GPS collector with advanced features
type ComprehensiveGPSCollectorInterface interface {
	GPSCollector

	// CollectBestGPS collects GPS data from the best available source (standardized format)
	CollectBestGPS(ctx context.Context) (*StandardizedGPSData, error)

	// CollectAllSources collects GPS data from all available sources for comparison
	CollectAllSources(ctx context.Context) (map[string]*StandardizedGPSData, error)

	// GetBestAvailableSource returns the name of the best available GPS source
	GetBestAvailableSource(ctx context.Context) string

	// GetSourceHealthStatus returns health status of all GPS sources
	GetSourceHealthStatus() map[string]GPSSourceHealth

	// ValidateGPSData validates standardized GPS data quality
	ValidateGPSData(gps *StandardizedGPSData) error
}

// NewGPSCollector creates a new GPS collector (uses comprehensive implementation)
func NewGPSCollector(logger *logx.Logger) ComprehensiveGPSCollectorInterface {
	config := DefaultComprehensiveGPSConfig()
	return NewComprehensiveGPSCollector(config, logger)
}

// NewGPSCollectorWithConfig creates a new GPS collector with custom configuration
func NewGPSCollectorWithConfig(config *ComprehensiveGPSConfig, logger *logx.Logger) ComprehensiveGPSCollectorInterface {
	return NewComprehensiveGPSCollector(config, logger)
}

// Implement the legacy interface for backward compatibility
func (gc *ComprehensiveGPSCollector) CollectGPS(ctx context.Context) (*pkg.GPSData, error) {
	standardized, err := gc.CollectBestGPS(ctx)
	if err != nil {
		return nil, err
	}

	return standardized.ConvertToLegacyFormat(), nil
}

func (gc *ComprehensiveGPSCollector) GetBestSource() string {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	return gc.GetBestAvailableSource(ctx)
}

func (gc *ComprehensiveGPSCollector) ValidateGPS(gps *pkg.GPSData) error {
	standardized := CreateStandardizedFromLegacy(gps)
	return gc.ValidateGPSData(standardized)
}
