package main

import (
	"fmt"
	"strings"
)

// LocationSourceComparison provides a comprehensive comparison of all location sources
type LocationSourceComparison struct {
	SourceName string
	Fields     map[string]FieldAvailability
}

type FieldAvailability struct {
	Available   bool
	FieldName   string
	DataType    string
	Description string
	Accuracy    string
	Notes       string
}

// GetAllLocationSources returns detailed comparison of all location sources
func GetAllLocationSources() []LocationSourceComparison {
	return []LocationSourceComparison{
		{
			SourceName: "GPS (Quectel GNSS)",
			Fields: map[string]FieldAvailability{
				"latitude":   {true, "Latitude", "float64", "Decimal degrees", "±2-5m", "Multi-constellation GNSS"},
				"longitude":  {true, "Longitude", "float64", "Decimal degrees", "±2-5m", "Multi-constellation GNSS"},
				"altitude":   {true, "Altitude", "float64", "Meters above sea level", "±5-10m", "Barometric + GNSS"},
				"accuracy":   {true, "HDOP * 5.0", "float64", "Calculated from HDOP", "±2-50m", "Horizontal Dilution of Precision"},
				"speed":      {true, "SpeedKmh", "float64", "Speed in km/h", "±0.1 km/h", "Doppler shift calculation"},
				"course":     {true, "Course", "float64", "Bearing in degrees", "±5°", "Direction of movement"},
				"satellites": {true, "Satellites", "int", "Number of satellites", "Exact", "All constellations (GPS/GLONASS/Galileo/BeiDou)"},
				"hdop":       {true, "HDOP", "float64", "Horizontal Dilution of Precision", "0.4-2.0", "Geometry quality indicator"},
				"timestamp":  {true, "Time", "string", "GPS time", "±1s", "UTC time from satellites"},
				"fix_type":   {true, "FixType", "int", "Fix quality (2D/3D)", "Exact", "2=2D, 3=3D fix"},
			},
		},
		{
			SourceName: "Starlink get_location",
			Fields: map[string]FieldAvailability{
				"latitude":   {true, "getLocation.lla.lat", "float64", "Decimal degrees", "±3-10m", "Starlink internal GPS/GNSS"},
				"longitude":  {true, "getLocation.lla.lon", "float64", "Decimal degrees", "±3-10m", "Starlink internal GPS/GNSS"},
				"altitude":   {true, "getLocation.lla.alt", "float64", "Meters above sea level", "±10-20m", "From Starlink dish GPS"},
				"accuracy":   {true, "getLocation.sigmaM", "float64", "Uncertainty in meters", "±5-50m", "Starlink's accuracy estimate"},
				"speed":      {true, "getLocation.horizontalSpeedMps", "float64", "Horizontal speed m/s", "±0.1 m/s", "Calculated from GPS"},
				"course":     {false, "N/A", "N/A", "Not provided", "N/A", "Static dish location"},
				"satellites": {false, "N/A", "N/A", "Not provided", "N/A", "Available in get_status"},
				"hdop":       {false, "N/A", "N/A", "Not provided", "N/A", "Uses sigmaM instead"},
				"timestamp":  {false, "N/A", "N/A", "Not provided", "N/A", "Can use system time"},
				"fix_type":   {true, "getLocation.source", "string", "GPS source type", "Exact", "GNC_NO_ACCEL, GNC_FUSED, etc."},
			},
		},
		{
			SourceName: "Starlink get_status",
			Fields: map[string]FieldAvailability{
				"latitude":   {false, "N/A", "N/A", "Not provided", "N/A", "Use get_location or get_diagnostics"},
				"longitude":  {false, "N/A", "N/A", "Not provided", "N/A", "Use get_location or get_diagnostics"},
				"altitude":   {false, "N/A", "N/A", "Not provided", "N/A", "Use get_location or get_diagnostics"},
				"accuracy":   {false, "N/A", "N/A", "Not provided", "N/A", "Use get_location sigmaM"},
				"speed":      {false, "N/A", "N/A", "Not provided", "N/A", "Use get_location"},
				"course":     {false, "N/A", "N/A", "Not provided", "N/A", "Static dish location"},
				"satellites": {true, "gpsStats.gpsSats", "int", "Number of GPS satellites", "Exact", "GPS satellite count"},
				"hdop":       {false, "N/A", "N/A", "Not provided", "N/A", "Use accuracy instead"},
				"timestamp":  {false, "N/A", "N/A", "Not provided", "N/A", "Can use system time"},
				"fix_type":   {true, "gpsStats.gpsValid", "bool", "GPS fix validity", "Exact", "true/false GPS status"},
			},
		},
		{
			SourceName: "Starlink get_diagnostics",
			Fields: map[string]FieldAvailability{
				"latitude":   {true, "location.latitude", "float64", "Decimal degrees", "±3-10m", "Starlink internal GPS/GNSS"},
				"longitude":  {true, "location.longitude", "float64", "Decimal degrees", "±3-10m", "Starlink internal GPS/GNSS"},
				"altitude":   {true, "location.altitudeMeters", "float64", "Meters above sea level", "±10-20m", "From Starlink dish GPS"},
				"accuracy":   {true, "location.uncertaintyMeters", "float64", "Uncertainty in meters", "±5-50m", "When uncertaintyMetersValid=true"},
				"speed":      {false, "N/A", "N/A", "Not provided", "N/A", "Use get_location"},
				"course":     {false, "N/A", "N/A", "Not provided", "N/A", "Static dish location"},
				"satellites": {false, "N/A", "N/A", "Not provided", "N/A", "Available in get_status"},
				"hdop":       {false, "N/A", "N/A", "Not provided", "N/A", "Uses uncertaintyMeters instead"},
				"timestamp":  {true, "location.gpsTimeS", "float64", "GPS time in seconds", "±1s", "GPS time from satellites"},
				"fix_type":   {true, "location.enabled", "bool", "Location enabled status", "Exact", "true/false location status"},
			},
		},
		{
			SourceName: "Google Geolocation API (WiFi)",
			Fields: map[string]FieldAvailability{
				"latitude":   {true, "Location.Lat", "float64", "Decimal degrees", "±10-100m", "WiFi access point triangulation"},
				"longitude":  {true, "Location.Lng", "float64", "Decimal degrees", "±10-100m", "WiFi access point triangulation"},
				"altitude":   {false, "N/A", "N/A", "Not provided", "N/A", "Can estimate ~50m above sea level"},
				"accuracy":   {true, "Accuracy", "float64", "Radius in meters (95% confidence)", "±10-500m", "Based on WiFi AP density"},
				"speed":      {false, "N/A", "N/A", "Not provided", "N/A", "Single point measurement"},
				"course":     {false, "N/A", "N/A", "Not provided", "N/A", "Single point measurement"},
				"satellites": {false, "N/A", "N/A", "Not applicable", "N/A", "Uses WiFi, not satellites"},
				"hdop":       {false, "N/A", "N/A", "Not applicable", "N/A", "Uses accuracy instead"},
				"timestamp":  {false, "N/A", "N/A", "Not provided", "N/A", "Can use system time"},
				"fix_type":   {true, "Derived", "string", "wifi/cellular/combined", "Exact", "Based on data sources used"},
			},
		},
		{
			SourceName: "Google Geolocation API (Cellular)",
			Fields: map[string]FieldAvailability{
				"latitude":   {true, "Location.Lat", "float64", "Decimal degrees", "±100-5000m", "Cell tower triangulation"},
				"longitude":  {true, "Location.Lng", "float64", "Decimal degrees", "±100-5000m", "Cell tower triangulation"},
				"altitude":   {false, "N/A", "N/A", "Not provided", "N/A", "Can estimate ~50m above sea level"},
				"accuracy":   {true, "Accuracy", "float64", "Radius in meters (95% confidence)", "±100-5000m", "Based on cell tower density"},
				"speed":      {false, "N/A", "N/A", "Not provided", "N/A", "Single point measurement"},
				"course":     {false, "N/A", "N/A", "Not provided", "N/A", "Single point measurement"},
				"satellites": {false, "N/A", "N/A", "Not applicable", "N/A", "Uses cell towers, not satellites"},
				"hdop":       {false, "N/A", "N/A", "Not applicable", "N/A", "Uses accuracy instead"},
				"timestamp":  {false, "N/A", "N/A", "Not provided", "N/A", "Can use system time"},
				"fix_type":   {true, "Derived", "string", "cellular/combined", "Exact", "Based on data sources used"},
			},
		},
		{
			SourceName: "Google Geolocation API (Combined)",
			Fields: map[string]FieldAvailability{
				"latitude":   {true, "Location.Lat", "float64", "Decimal degrees", "±20-200m", "WiFi + Cellular triangulation"},
				"longitude":  {true, "Location.Lng", "float64", "Decimal degrees", "±20-200m", "WiFi + Cellular triangulation"},
				"altitude":   {false, "N/A", "N/A", "Not provided", "N/A", "Can estimate ~50m above sea level"},
				"accuracy":   {true, "Accuracy", "float64", "Radius in meters (95% confidence)", "±20-1000m", "Best of WiFi + Cellular"},
				"speed":      {false, "N/A", "N/A", "Not provided", "N/A", "Single point measurement"},
				"course":     {false, "N/A", "N/A", "Not provided", "N/A", "Single point measurement"},
				"satellites": {false, "N/A", "N/A", "Not applicable", "N/A", "Uses WiFi + Cellular"},
				"hdop":       {false, "N/A", "N/A", "Not applicable", "N/A", "Uses accuracy instead"},
				"timestamp":  {false, "N/A", "N/A", "Not provided", "N/A", "Can use system time"},
				"fix_type":   {true, "Derived", "string", "combined", "Exact", "WiFi + Cellular data sources"},
			},
		},
	}
}

// PrintLocationSourceComparison prints a detailed comparison table
func PrintLocationSourceComparison() {
	fmt.Println("📊 COMPREHENSIVE LOCATION SOURCE COMPARISON")
	fmt.Println("===========================================")

	sources := GetAllLocationSources()

	// Define the fields we want to compare
	compareFields := []string{"latitude", "longitude", "altitude", "accuracy", "speed", "course", "satellites", "hdop", "timestamp", "fix_type"}

	// Print header
	fmt.Printf("%-12s", "Field")
	for _, source := range sources {
		fmt.Printf(" | %-20s", truncateString(source.SourceName, 20))
	}
	fmt.Println()
	fmt.Println(strings.Repeat("=", 12+len(sources)*23))

	// Print each field comparison
	for _, field := range compareFields {
		fmt.Printf("%-12s", strings.Title(field))
		for _, source := range sources {
			if fieldInfo, exists := source.Fields[field]; exists {
				if fieldInfo.Available {
					fmt.Printf(" | %-20s", "✅ "+truncateString(fieldInfo.FieldName, 17))
				} else {
					fmt.Printf(" | %-20s", "❌ N/A")
				}
			} else {
				fmt.Printf(" | %-20s", "❌ N/A")
			}
		}
		fmt.Println()
	}

	fmt.Println("\n🎯 FIELD AVAILABILITY SUMMARY:")
	fmt.Println("==============================")

	for _, source := range sources {
		fmt.Printf("\n📍 %s:\n", source.SourceName)
		available := 0
		total := len(compareFields)

		for _, field := range compareFields {
			if fieldInfo, exists := source.Fields[field]; exists && fieldInfo.Available {
				fmt.Printf("  ✅ %-12s: %s (%s) - %s\n",
					strings.Title(field),
					fieldInfo.FieldName,
					fieldInfo.Accuracy,
					fieldInfo.Notes)
				available++
			} else {
				fmt.Printf("  ❌ %-12s: Not available\n", strings.Title(field))
			}
		}
		fmt.Printf("  📊 Coverage: %d/%d fields (%.0f%%)\n", available, total, float64(available)/float64(total)*100)
	}
}

// PrintGapAnalysis identifies gaps and compensation strategies
func PrintGapAnalysis() {
	fmt.Println("\n🔍 GAP ANALYSIS & COMPENSATION STRATEGIES")
	fmt.Println("=========================================")

	fmt.Println("\n📋 CRITICAL GAPS IDENTIFIED:")

	gaps := []struct {
		field       string
		description string
		impact      string
		solutions   []string
	}{
		{
			"altitude",
			"Google API doesn't provide altitude",
			"HIGH - Many applications expect altitude data",
			[]string{
				"✅ Use GPS/Starlink altitude when available",
				"✅ Estimate ~50m above sea level as default",
				"✅ Use barometric pressure sensor if available",
				"⚠️  Cache last known altitude from GPS",
			},
		},
		{
			"speed",
			"Only GPS provides speed data",
			"MEDIUM - Movement detection and navigation apps",
			[]string{
				"✅ Calculate speed from position changes over time",
				"✅ Use GPS speed when available",
				"⚠️  Set to 0.0 for static locations (Starlink)",
				"❌ Cannot estimate from single API calls",
			},
		},
		{
			"course",
			"Only GPS provides bearing/course data",
			"MEDIUM - Navigation and movement tracking",
			[]string{
				"✅ Calculate bearing from position changes over time",
				"✅ Use GPS course when available",
				"⚠️  Set to 0.0 for static locations",
				"❌ Cannot estimate from single API calls",
			},
		},
		{
			"satellites",
			"Only GPS provides satellite count",
			"LOW - Quality indicator for GPS reliability",
			[]string{
				"✅ Use GPS satellite count when available",
				"✅ Set to nil for non-GPS sources",
				"⚠️  Use accuracy as quality indicator instead",
				"❌ Cannot simulate for API sources",
			},
		},
		{
			"hdop",
			"Only GPS provides HDOP",
			"LOW - GPS quality indicator",
			[]string{
				"✅ Use GPS HDOP when available",
				"✅ Convert accuracy to estimated HDOP (accuracy/5.0)",
				"✅ Set to nil for non-GPS sources",
				"⚠️  Use accuracy as primary quality metric",
			},
		},
		{
			"timestamp",
			"GPS provides GPS time, others use system time",
			"MEDIUM - Time synchronization and data correlation",
			[]string{
				"✅ Use GPS time when available (most accurate)",
				"✅ Use system time for API sources",
				"✅ Add timestamp when creating response",
				"⚠️  Consider time zone handling",
			},
		},
	}

	for i, gap := range gaps {
		fmt.Printf("\n%d. 🚨 %s\n", i+1, strings.ToUpper(gap.field))
		fmt.Printf("   📝 Issue: %s\n", gap.description)
		fmt.Printf("   💥 Impact: %s\n", gap.impact)
		fmt.Printf("   🛠️  Solutions:\n")
		for _, solution := range gap.solutions {
			fmt.Printf("      %s\n", solution)
		}
	}

	fmt.Println("\n🎯 COMPENSATION STRATEGIES:")
	fmt.Println("===========================")

	strategies := []struct {
		strategy    string
		description string
		priority    string
	}{
		{
			"Hierarchical Fallback",
			"GPS → Starlink → Google Combined → Google WiFi → Google Cellular",
			"HIGH",
		},
		{
			"Field Simulation",
			"Estimate missing fields using available data and defaults",
			"HIGH",
		},
		{
			"Temporal Calculation",
			"Calculate speed/course from position changes over time",
			"MEDIUM",
		},
		{
			"Quality Scoring",
			"Use accuracy + data source to create unified quality score",
			"MEDIUM",
		},
		{
			"Caching Strategy",
			"Cache GPS-derived fields (altitude, speed, course) for API fallbacks",
			"MEDIUM",
		},
		{
			"Standardized Response",
			"Always return same structure with nil for unavailable fields",
			"HIGH",
		},
	}

	for i, strategy := range strategies {
		fmt.Printf("%d. 🎯 %s (%s Priority)\n", i+1, strategy.strategy, strategy.priority)
		fmt.Printf("   📝 %s\n\n", strategy.description)
	}
}

// PrintRecommendedImplementation shows the recommended approach
func PrintRecommendedImplementation() {
	fmt.Println("🛰️  STARLINK MULTI-API STRATEGY")
	fmt.Println("===============================")

	fmt.Println("\n📡 Starlink provides GPS data in THREE different APIs:")
	fmt.Println("   1️⃣  get_location - 🎯 BEST for coordinates (lat/lon/alt + speed + accuracy)")
	fmt.Println("   2️⃣  get_diagnostics - ⏰ BEST for timestamp (GPS time + coordinates)")
	fmt.Println("   3️⃣  get_status - 🛰️  BEST for satellite info (satellite count + GPS validity)")

	fmt.Println("\n🔄 OPTIMAL STARLINK COMBINATION STRATEGY:")
	fmt.Println("   ✅ Primary: get_location for coordinates, altitude, speed, accuracy")
	fmt.Println("   ✅ Supplement: get_status for satellite count and GPS validity")
	fmt.Println("   ✅ Optional: get_diagnostics for GPS timestamp when needed")
	fmt.Println("   ✅ Result: Near-complete GPS dataset from Starlink APIs")

	fmt.Println("\n🚀 RECOMMENDED IMPLEMENTATION APPROACH")
	fmt.Println("======================================")

	fmt.Println("\n1. 📋 STANDARDIZED RESPONSE STRUCTURE:")
	fmt.Println("   ✅ Always return same struct with optional fields as pointers")
	fmt.Println("   ✅ Use nil for unavailable fields (altitude, speed, course, satellites, hdop)")
	fmt.Println("   ✅ Always provide: latitude, longitude, accuracy, timestamp, source")
	fmt.Println("   ✅ Include metadata: fix_type, confidence, from_cache, api_cost")

	fmt.Println("\n2. 🔄 FIELD COMPENSATION LOGIC:")
	fmt.Println("   ✅ Altitude: GPS/Starlink → Estimate 50m → nil")
	fmt.Println("   ✅ Speed: GPS/Starlink get_location → Calculate from movement → 0.0 for static → nil")
	fmt.Println("   ✅ Course: GPS → Calculate from movement → 0.0 for static → nil")
	fmt.Println("   ✅ Satellites: GPS/Starlink get_status → nil for others")
	fmt.Println("   ✅ HDOP: GPS only → nil for others (use accuracy instead)")
	fmt.Println("   ✅ Timestamp: GPS time/Starlink get_diagnostics → System time")

	fmt.Println("\n3. 🎯 QUALITY ASSESSMENT:")
	fmt.Println("   ✅ GPS: Use satellites + HDOP for confidence")
	fmt.Println("   ✅ Starlink: Use sigmaM + satellite count (multi-API) for confidence")
	fmt.Println("   ✅ Google API: Use accuracy + data source count for confidence")
	fmt.Println("   ✅ Unified confidence score: 0.0-1.0")

	fmt.Println("\n4. 🔄 ENHANCED FALLBACK HIERARCHY:")
	fmt.Println("   1️⃣  GPS (Quectel) - Most complete single-source data")
	fmt.Println("   2️⃣  Starlink Combined (get_location + get_status) - Near-complete multi-API data")
	fmt.Println("   3️⃣  Starlink get_location - Good accuracy, some fields")
	fmt.Println("   4️⃣  Google Combined (WiFi+Cellular) - Best API accuracy")
	fmt.Println("   5️⃣  Google WiFi - Good urban accuracy")
	fmt.Println("   6️⃣  Google Cellular - Wide coverage, lower accuracy")

	fmt.Println("\n5. 💾 CACHING STRATEGY:")
	fmt.Println("   ✅ Cache complete GPS data for field simulation")
	fmt.Println("   ✅ Use intelligent cache invalidation (cellular environment changes)")
	fmt.Println("   ✅ Adaptive intervals (5min moving, 10-60min stationary)")
	fmt.Println("   ✅ Quality gating to prevent jumpy updates")

	fmt.Println("\n6. 🛠️  DEPENDENCY HANDLING:")
	fmt.Println("   ✅ Applications expecting altitude: Provide estimated or cached value")
	fmt.Println("   ✅ Applications expecting speed/course: Calculate from movement or set nil")
	fmt.Println("   ✅ Applications expecting satellites: Use confidence score instead")
	fmt.Println("   ✅ Applications expecting HDOP: Use accuracy as quality indicator")
}

func truncateString(s string, maxLen int) string {
	if len(s) <= maxLen {
		return s
	}
	return s[:maxLen-3] + "..."
}

// testLocationSourceComparison demonstrates the comparison system
func testLocationSourceComparison() {
	fmt.Println("📊 Location Source Comparison Test")
	fmt.Println("==================================")

	PrintLocationSourceComparison()
	PrintGapAnalysis()
	PrintRecommendedImplementation()

	fmt.Println("\n🎯 KEY TAKEAWAYS:")
	fmt.Println("=================")
	fmt.Println("✅ GPS (Quectel) provides the most complete single-source dataset (10/10 fields)")
	fmt.Println("✅ Starlink Multi-API provides near-complete dataset (8/10 fields combined)")
	fmt.Println("   📍 get_location: coordinates + altitude + speed + accuracy")
	fmt.Println("   🛰️  get_status: satellite count + GPS validity")
	fmt.Println("   ⏰ get_diagnostics: GPS timestamp + coordinates")
	fmt.Println("✅ Google API provides accurate location but minimal fields (3/10 fields)")
	fmt.Println("✅ Field compensation and standardized responses solve compatibility issues")
	fmt.Println("✅ Intelligent caching and fallback hierarchy optimize accuracy and cost")
	fmt.Println("✅ Quality scoring provides unified reliability assessment across all sources")
	fmt.Println("🚀 Starlink multi-API strategy significantly improves GPS data completeness!")
}
