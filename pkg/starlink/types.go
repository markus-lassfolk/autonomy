package starlink

import (
	"encoding/json"
	"time"
)

// StatusResponse represents the response from get_status method
type StatusResponse struct {
	DishGetStatus struct {
		// Device Information
		DeviceInfo struct {
			ID                 string `json:"id"`
			HardwareVersion    string `json:"hardwareVersion"`
			SoftwareVersion    string `json:"softwareVersion"`
			CountryCode        string `json:"countryCode"`
			GenerationNumber   string `json:"generationNumber"` // This comes as string from API
			BootCount          int    `json:"bootCount"`
			SoftwarePartNumber string `json:"softwarePartNumber"`
			UTCOffsetS         int32  `json:"utcOffsetS"`
		} `json:"deviceInfo"`

		// Device State
		DeviceState struct {
			UptimeS string `json:"uptimeS"` // This comes as string from API
		} `json:"deviceState"`

		// Obstruction Statistics
		ObstructionStats struct {
			CurrentlyObstructed              bool      `json:"currentlyObstructed"`
			FractionObstructed               float64   `json:"fractionObstructed"`
			Last24hObstructedS               int       `json:"last24hObstructedS"`
			ValidS                           int       `json:"validS"`
			WedgeFractionObstructed          []float64 `json:"wedgeFractionObstructed"`
			WedgeAbsFractionObstructed       []float64 `json:"wedgeAbsFractionObstructed"`
			TimeObstructed                   float64   `json:"timeObstructed"`
			PatchesValid                     int       `json:"patchesValid"`
			AvgProlongedObstructionIntervalS string    `json:"avgProlongedObstructionIntervalS"` // Can be "NaN"
		} `json:"obstructionStats"`

		// Network Performance
		PopPingLatencyMs      float64 `json:"popPingLatencyMs"`
		DownlinkThroughputBps float64 `json:"downlinkThroughputBps"`
		UplinkThroughputBps   float64 `json:"uplinkThroughputBps"`
		PopPingDropRate       float64 `json:"popPingDropRate"`
		EthSpeedMbps          int32   `json:"ethSpeedMbps"`

		// Signal Quality
		SNR                  float64 `json:"snr"`
		IsSnrAboveNoiseFloor bool    `json:"isSnrAboveNoiseFloor"`
		IsSnrPersistentlyLow bool    `json:"isSnrPersistentlyLow"`

		// GPS Information
		GPSStats struct {
			GPSValid        bool `json:"gpsValid"`
			GPSSats         int  `json:"gpsSats"`
			NoSatsAfterTtff int  `json:"noSatsAfterTtff"`
			InhibitGPS      bool `json:"inhibitGps"`
		} `json:"gpsStats"`

		// Dish Orientation
		BoresightAzimuthDeg   float64 `json:"boresightAzimuthDeg"`
		BoresightElevationDeg float64 `json:"boresightElevationDeg"`

		// System Information
		MobilityClass              string `json:"mobilityClass"`
		ClassOfService             string `json:"classOfService"`
		SoftwareUpdateState        string `json:"softwareUpdateState"`
		SwupdateRebootReady        bool   `json:"swupdateRebootReady"`
		SecondsToFirstNonemptySlot int    `json:"secondsToFirstNonemptySlot"`
	} `json:"dishGetStatus"`
}

// LocationResponse represents the response from get_location method
type LocationResponse struct {
	GetLocation struct {
		LLA struct {
			Lat float64 `json:"lat"`
			Lon float64 `json:"lon"`
			Alt float64 `json:"alt"`
		} `json:"lla"`
		SigmaM float64 `json:"sigmaM"` // Accuracy in meters
		Source string  `json:"source"` // GPS source (e.g., "GNC_FUSED")
	} `json:"getLocation"`
}

// DiagnosticsResponse represents the response from get_diagnostics method
type DiagnosticsResponse struct {
	DishGetDiagnostics struct {
		// Hardware diagnostics - handle both array and object responses
		Alerts json.RawMessage `json:"alerts"`

		// Temperature information
		DisablementCode string  `json:"disablementCode"`
		ThermalThrottle bool    `json:"thermalThrottle"`
		ThermalShutdown bool    `json:"thermalShutdown"`
		Temperature     float64 `json:"temperature"`

		// Power information
		PowerSupplyTemp float64 `json:"powerSupplyTemp"`
		PowerState      string  `json:"powerState"`

		// Network diagnostics
		EthernetSpeed   int32 `json:"ethernetSpeed"`
		EthernetDuplex  bool  `json:"ethernetDuplex"`
		EthernetAutoneg bool  `json:"ethernetAutoneg"`
	} `json:"dishGetDiagnostics"`
}

// HealthData represents comprehensive health information
type HealthData struct {
	Timestamp   time.Time            `json:"timestamp"`
	Status      *StatusResponse      `json:"status"`
	Diagnostics *DiagnosticsResponse `json:"diagnostics,omitempty"`
	IsHealthy   bool                 `json:"is_healthy"`
	Issues      []string             `json:"issues"`
}

// DeviceInfo represents Starlink device information
type DeviceInfo struct {
	ID                 string `json:"id"`
	HardwareVersion    string `json:"hardware_version"`
	SoftwareVersion    string `json:"software_version"`
	CountryCode        string `json:"country_code"`
	GenerationNumber   int32  `json:"generation_number"`
	BootCount          int    `json:"boot_count"`
	SoftwarePartNumber string `json:"software_part_number"`
	UTCOffsetS         int32  `json:"utc_offset_s"`
}

// ObstructionInfo represents obstruction statistics
type ObstructionInfo struct {
	CurrentlyObstructed              bool      `json:"currently_obstructed"`
	FractionObstructed               float64   `json:"fraction_obstructed"`
	Last24hObstructedS               int       `json:"last_24h_obstructed_s"`
	ValidS                           int       `json:"valid_s"`
	WedgeFractionObstructed          []float64 `json:"wedge_fraction_obstructed"`
	WedgeAbsFractionObstructed       []float64 `json:"wedge_abs_fraction_obstructed"`
	TimeObstructed                   float64   `json:"time_obstructed"`
	PatchesValid                     int       `json:"patches_valid"`
	AvgProlongedObstructionIntervalS float64   `json:"avg_prolonged_obstruction_interval_s"`
}

// NetworkPerformance represents network performance metrics
type NetworkPerformance struct {
	PopPingLatencyMs      float64 `json:"pop_ping_latency_ms"`
	DownlinkThroughputBps float64 `json:"downlink_throughput_bps"`
	UplinkThroughputBps   float64 `json:"uplink_throughput_bps"`
	PopPingDropRate       float64 `json:"pop_ping_drop_rate"`
	EthSpeedMbps          int32   `json:"eth_speed_mbps"`
}

// SignalQuality represents signal quality metrics
type SignalQuality struct {
	SNR                  float64 `json:"snr"`
	IsSnrAboveNoiseFloor bool    `json:"is_snr_above_noise_floor"`
	IsSnrPersistentlyLow bool    `json:"is_snr_persistently_low"`
}

// GPSInfo represents GPS information from Starlink
type GPSInfo struct {
	GPSValid        bool `json:"gps_valid"`
	GPSSats         int  `json:"gps_sats"`
	NoSatsAfterTtff int  `json:"no_sats_after_ttff"`
	InhibitGPS      bool `json:"inhibit_gps"`
}

// DishOrientation represents dish pointing information
type DishOrientation struct {
	BoresightAzimuthDeg   float64 `json:"boresight_azimuth_deg"`
	BoresightElevationDeg float64 `json:"boresight_elevation_deg"`
}

// SystemInfo represents system-level information
type SystemInfo struct {
	MobilityClass              string `json:"mobility_class"`
	ClassOfService             string `json:"class_of_service"`
	SoftwareUpdateState        string `json:"software_update_state"`
	SwupdateRebootReady        bool   `json:"swupdate_reboot_ready"`
	SecondsToFirstNonemptySlot int    `json:"seconds_to_first_nonempty_slot"`
	UptimeS                    uint64 `json:"uptime_s"`
}
