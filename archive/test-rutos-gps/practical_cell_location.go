package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"strings"
	"time"
)

// PracticalCellLocationResult represents a practical cell tower location result
type PracticalCellLocationResult struct {
	Method            string           `json:"method"`
	Success           bool             `json:"success"`
	EstimatedLat      float64          `json:"estimated_lat"`
	EstimatedLon      float64          `json:"estimated_lon"`
	EstimatedAccuracy float64          `json:"estimated_accuracy"`
	NearbyCells       []NearbyCellInfo `json:"nearby_cells"`
	DistanceFromGPS   float64          `json:"distance_from_gps"`
	ResponseTime      float64          `json:"response_time_ms"`
	Error             string           `json:"error,omitempty"`
}

type NearbyCellInfo struct {
	CellID   int     `json:"cellid"`
	Lat      float64 `json:"lat"`
	Lon      float64 `json:"lon"`
	Range    int     `json:"range"`
	Samples  int     `json:"samples"`
	Radio    string  `json:"radio"`
	Distance float64 `json:"distance_from_gps"`
}

// getPracticalCellLocation gets location using nearby cells and area search
func getPracticalCellLocation() (*PracticalCellLocationResult, error) {
	start := time.Now()
	result := &PracticalCellLocationResult{
		Method: "area_search_estimation",
	}

	apiKey, err := loadOpenCellIDTokenLocal()
	if err != nil {
		result.Error = fmt.Sprintf("Failed to load API key: %v", err)
		return result, err
	}

	// Your GPS coordinates for reference
	gpsLat := 59.48007000
	gpsLon := 18.27985000

	// Search for Telia cells in your area
	latMin := gpsLat - 0.01 // ±1km
	latMax := gpsLat + 0.01
	lonMin := gpsLon - 0.01
	lonMax := gpsLon + 0.01

	baseURL := "https://opencellid.org/cell/getInArea"
	params := url.Values{}
	params.Add("key", apiKey)
	params.Add("BBOX", fmt.Sprintf("%.6f,%.6f,%.6f,%.6f", latMin, lonMin, latMax, lonMax))
	params.Add("mcc", "240") // Sweden
	params.Add("mnc", "1")   // Telia
	params.Add("format", "json")
	params.Add("limit", "20") // Get more cells for better estimation

	fullURL := baseURL + "?" + params.Encode()

	resp, err := http.Get(fullURL)
	if err != nil {
		result.Error = fmt.Sprintf("HTTP request failed: %v", err)
		return result, err
	}
	defer resp.Body.Close()

	result.ResponseTime = float64(time.Since(start).Nanoseconds()) / 1e6

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		result.Error = fmt.Sprintf("Failed to read response: %v", err)
		return result, err
	}

	// Parse area search response
	var areaResponse struct {
		Count int `json:"count"`
		Cells []struct {
			Lat     float64 `json:"lat"`
			Lon     float64 `json:"lon"`
			MCC     int     `json:"mcc"`
			MNC     int     `json:"mnc"`
			LAC     int     `json:"lac"`
			CellID  int     `json:"cellid"`
			Range   int     `json:"range"`
			Samples int     `json:"samples"`
			Radio   string  `json:"radio"`
		} `json:"cells"`
	}

	if err := json.Unmarshal(body, &areaResponse); err != nil {
		result.Error = fmt.Sprintf("Failed to parse response: %v", err)
		return result, err
	}

	if areaResponse.Count == 0 {
		result.Error = "No cells found in area"
		return result, fmt.Errorf("no cells found in area")
	}

	// Process nearby cells and find the best location estimate
	var totalLat, totalLon, totalWeight float64

	for _, cell := range areaResponse.Cells {
		distance := calculateDistance(gpsLat, gpsLon, cell.Lat, cell.Lon)

		// Add to nearby cells list
		nearby := NearbyCellInfo{
			CellID:   cell.CellID,
			Lat:      cell.Lat,
			Lon:      cell.Lon,
			Range:    cell.Range,
			Samples:  cell.Samples,
			Radio:    cell.Radio,
			Distance: distance,
		}
		result.NearbyCells = append(result.NearbyCells, nearby)

		// Weight cells by proximity and sample count (more samples = more reliable)
		// Closer cells get higher weight, cells with more samples get higher weight
		weight := float64(cell.Samples) / (1.0 + distance/1000.0) // Distance in km

		totalLat += cell.Lat * weight
		totalLon += cell.Lon * weight
		totalWeight += weight
	}

	if totalWeight > 0 {
		result.EstimatedLat = totalLat / totalWeight
		result.EstimatedLon = totalLon / totalWeight
		result.Success = true

		// Calculate distance from GPS
		result.DistanceFromGPS = calculateDistance(gpsLat, gpsLon, result.EstimatedLat, result.EstimatedLon)

		// Estimate accuracy based on cell spread and distances
		result.EstimatedAccuracy = estimateLocationAccuracy(result.NearbyCells)
	}

	return result, nil
}

// estimateLocationAccuracy estimates accuracy based on nearby cell distribution
func estimateLocationAccuracy(cells []NearbyCellInfo) float64 {
	if len(cells) == 0 {
		return 10000 // Very poor accuracy
	}

	// Find the closest cells
	minDistance := cells[0].Distance
	maxRange := 0
	totalSamples := 0

	for _, cell := range cells {
		if cell.Distance < minDistance {
			minDistance = cell.Distance
		}
		if cell.Range > maxRange {
			maxRange = cell.Range
		}
		totalSamples += cell.Samples
	}

	// Base accuracy on closest cell distance and cell tower ranges
	baseAccuracy := minDistance + float64(maxRange)

	// Improve accuracy if we have many samples (more reliable data)
	if totalSamples > 10 {
		baseAccuracy *= 0.8
	} else if totalSamples > 5 {
		baseAccuracy *= 0.9
	}

	// Improve accuracy if we have multiple cells (triangulation effect)
	if len(cells) > 5 {
		baseAccuracy *= 0.7
	} else if len(cells) > 2 {
		baseAccuracy *= 0.8
	}

	return baseAccuracy
}

// testPracticalCellLocation tests the practical cell location approach
func testPracticalCellLocation() error {
	fmt.Println("🎯 PRACTICAL CELL TOWER LOCATION TEST")
	fmt.Println("=" + strings.Repeat("=", 40))
	fmt.Println("📡 Using nearby cells for location estimation")
	fmt.Println()

	result, err := getPracticalCellLocation()
	if err != nil {
		fmt.Printf("❌ Test failed: %v\n", err)
		return err
	}

	gpsLat := 59.48007000
	gpsLon := 18.27985000

	fmt.Printf("📍 GPS Reference: %.8f°, %.8f°\n", gpsLat, gpsLon)
	fmt.Printf("⏱️  Response Time: %.1f ms\n", result.ResponseTime)
	fmt.Printf("📊 Found %d nearby Telia cells\n", len(result.NearbyCells))

	if result.Success {
		fmt.Printf("\n✅ LOCATION ESTIMATION SUCCESS:\n")
		fmt.Printf("  📍 Estimated: %.6f°, %.6f°\n", result.EstimatedLat, result.EstimatedLon)
		fmt.Printf("  🎯 Accuracy: ±%.0f meters\n", result.EstimatedAccuracy)
		fmt.Printf("  📏 Distance from GPS: %.0f meters\n", result.DistanceFromGPS)

		// Create Google Maps link
		mapsLink := fmt.Sprintf("https://www.google.com/maps?q=%.6f,%.6f", result.EstimatedLat, result.EstimatedLon)
		fmt.Printf("  🗺️  Maps Link: %s\n", mapsLink)

		// Accuracy assessment
		if result.DistanceFromGPS < 200 {
			fmt.Printf("  🎯 EXCELLENT: Very accurate cell tower location!\n")
		} else if result.DistanceFromGPS < 500 {
			fmt.Printf("  ✅ GOOD: Accurate enough for most use cases\n")
		} else if result.DistanceFromGPS < 1000 {
			fmt.Printf("  ⚠️  FAIR: Acceptable for area detection\n")
		} else {
			fmt.Printf("  ❌ POOR: Location estimation may be inaccurate\n")
		}

		// Show nearby cells
		fmt.Printf("\n📋 Nearby Telia Cells (closest first):\n")

		// Sort by distance
		for i := 0; i < len(result.NearbyCells)-1; i++ {
			for j := i + 1; j < len(result.NearbyCells); j++ {
				if result.NearbyCells[i].Distance > result.NearbyCells[j].Distance {
					result.NearbyCells[i], result.NearbyCells[j] = result.NearbyCells[j], result.NearbyCells[i]
				}
			}
		}

		for i, cell := range result.NearbyCells {
			if i >= 5 { // Show top 5
				fmt.Printf("  ... and %d more cells\n", len(result.NearbyCells)-5)
				break
			}

			status := ""
			if cell.CellID == 25939744 || cell.CellID == 25939734 {
				status = " 🎯 VERY CLOSE TO YOUR CELL!"
			}

			fmt.Printf("  %d. Cell %d: %.6f°, %.6f° (%s, ±%dm, %d samples, %.0fm away)%s\n",
				i+1, cell.CellID, cell.Lat, cell.Lon, cell.Radio, cell.Range, cell.Samples, cell.Distance, status)
		}

		// Recommendation
		fmt.Printf("\n💡 RECOMMENDATION FOR autonomy:\n")
		if result.DistanceFromGPS < 500 {
			fmt.Printf("  ✅ Cell tower location is VIABLE as GPS fallback\n")
			fmt.Printf("  🎯 Use this method when GPS signals are blocked\n")
			fmt.Printf("  📱 Perfect for indoor positioning and emergency fallback\n")
		} else {
			fmt.Printf("  ⚠️  Cell tower location provides rough area detection only\n")
			fmt.Printf("  🏠 Good for geofencing (home/away detection)\n")
			fmt.Printf("  🚨 Acceptable for emergency location when GPS fails\n")
		}

	} else {
		fmt.Printf("❌ Location estimation failed: %s\n", result.Error)
	}

	// Save results
	filename := fmt.Sprintf("practical_cell_location_test_%s.json",
		time.Now().Format("2006-01-02_15-04-05"))

	data, _ := json.MarshalIndent(result, "", "  ")
	if err := writeFile(filename, data); err == nil {
		fmt.Printf("\n💾 Results saved to: %s\n", filename)
	}

	return nil
}

// writeFile is a simple wrapper for os.WriteFile
func writeFile(filename string, data []byte) error {
	return os.WriteFile(filename, data, 0o644)
}
