package pkg

import (
	"context"
	"time"
)

// Member class constants
const (
	MemberClassStarlink = "starlink"
	MemberClassCellular = "cellular"
	MemberClassWiFi     = "wifi"
	MemberClassLAN      = "lan"
	MemberClassGeneric  = "generic"
)

// Event type constants
const (
	EventTypeSwitch   = "switch"
	EventTypeFailover = "failover"
	EventTypeRestore  = "restore"
	EventTypeError    = "error"
)

// Member represents a network interface member that can be monitored and controlled
type Member struct {
	Name            string            `json:"name"`
	Class           InterfaceClass    `json:"class"`
	Iface           string            `json:"iface"`
	Policy          string            `json:"policy,omitempty"`
	Weight          int               `json:"weight"`
	Eligible        bool              `json:"eligible"`
	Detect          string            `json:"detect"` // auto|disable|force
	Config          map[string]string `json:"config,omitempty"`
	LastSeen        time.Time         `json:"last_seen"`
	CreatedAt       time.Time         `json:"created_at"`
	IsPrimary       bool              `json:"is_primary"`                  // Whether this is the primary/active interface
	DataLimitConfig interface{}       `json:"data_limit_config,omitempty"` // Data limit configuration (will be cast to *discovery.DataLimitConfig)
}

// Metrics represents the health metrics for a member
type Metrics struct {
	Timestamp      time.Time `json:"timestamp"`
	LatencyMS      *float64  `json:"lat_ms,omitempty"`
	LossPercent    *float64  `json:"loss_pct,omitempty"`
	JitterMS       *float64  `json:"jitter_ms,omitempty"`
	CollectedAt    time.Time `json:"collected_at"`
	TargetHost     *string   `json:"target_host,omitempty"`
	ProbeMethod    *string   `json:"probe_method,omitempty"`
	RXBytes        *uint64   `json:"rx_bytes,omitempty"`
	TXBytes        *uint64   `json:"tx_bytes,omitempty"`
	ObstructionPct *float64  `json:"obstruction_pct,omitempty"` // Starlink only
	Outages        *int      `json:"outages,omitempty"`         // Starlink only
	RSRP           *float64  `json:"rsrp,omitempty"`            // Cellular only (dBm)
	RSRQ           *float64  `json:"rsrq,omitempty"`            // Cellular only (dB)
	SINR           *float64  `json:"sinr,omitempty"`            // Cellular only (dB)
	SignalStrength *int      `json:"signal,omitempty"`          // WiFi only
	NoiseLevel     *int      `json:"noise,omitempty"`           // WiFi only
	SNR            *int      `json:"snr,omitempty"`             // WiFi only
	Bitrate        *int      `json:"bitrate,omitempty"`         // WiFi only
	Quality        *int      `json:"quality,omitempty"`         // WiFi quality percentage
	LinkQuality    *int      `json:"link_quality,omitempty"`    // WiFi link quality
	TxPower        *int      `json:"tx_power,omitempty"`        // WiFi transmission power
	Frequency      *int      `json:"frequency,omitempty"`       // WiFi frequency in MHz
	Channel        *int      `json:"channel,omitempty"`         // WiFi channel number
	TetheringMode  *bool     `json:"tethering_mode,omitempty"`  // WiFi tethering/AP mode
	NetworkType    *string   `json:"network_type,omitempty"`    // Cellular only
	Roaming        *bool     `json:"roaming,omitempty"`         // Cellular only
	Operator       *string   `json:"operator,omitempty"`        // Cellular only
	Band           *string   `json:"band,omitempty"`            // Cellular only
	CellID         *string   `json:"cell_id,omitempty"`         // Cellular only

	// Enhanced Starlink Diagnostics
	UptimeS                     *int64  `json:"uptime_s,omitempty"`                       // Device uptime in seconds
	BootCount                   *int    `json:"boot_count,omitempty"`                     // Boot counter
	IsSNRAboveNoiseFloor        *bool   `json:"is_snr_above_noise_floor,omitempty"`       // Signal above noise threshold
	IsSNRPersistentlyLow        *bool   `json:"is_snr_persistently_low,omitempty"`        // Chronic signal issues
	HardwareSelfTest            *string `json:"hardware_self_test,omitempty"`             // Hardware test result
	DLBandwidthRestrictedReason *string `json:"dl_bandwidth_restricted_reason,omitempty"` // Downlink restrictions
	ULBandwidthRestrictedReason *string `json:"ul_bandwidth_restricted_reason,omitempty"` // Uplink restrictions
	ThermalThrottle             *bool   `json:"thermal_throttle,omitempty"`               // Thermal throttling active
	ThermalShutdown             *bool   `json:"thermal_shutdown,omitempty"`               // Thermal shutdown imminent
	RoamingAlert                *bool   `json:"roaming_alert,omitempty"`                  // Roaming status
	SoftwareUpdateState         *string `json:"software_update_state,omitempty"`          // Software update state
	RebootScheduledUTC          *string `json:"reboot_scheduled_utc,omitempty"`           // Scheduled reboot time
	SwupdateRebootReady         *bool   `json:"swupdate_reboot_ready,omitempty"`          // Software update ready for reboot

	// Starlink Events (for enhanced scoring and predictive analysis)
	Events *[]StarlinkEvent `json:"events,omitempty"` // Recent Starlink events

	// Enhanced Obstruction Data
	ObstructionTimePct      *float64 `json:"obstruction_time_pct,omitempty"`      // Historical obstruction time
	ObstructionValidS       *int64   `json:"obstruction_valid_s,omitempty"`       // Valid measurement duration
	ObstructionAvgProlonged *float64 `json:"obstruction_avg_prolonged,omitempty"` // Average prolonged obstruction
	ObstructionPatchesValid *int     `json:"obstruction_patches_valid,omitempty"` // Valid measurement patches

	// GPS Data
	GPSValid             *bool    `json:"gps_valid,omitempty"`              // GPS fix validity
	GPSSatellites        *int     `json:"gps_satellites,omitempty"`         // Number of satellites
	GPSLatitude          *float64 `json:"gps_latitude,omitempty"`           // GPS latitude
	GPSLongitude         *float64 `json:"gps_longitude,omitempty"`          // GPS longitude
	GPSAltitude          *float64 `json:"gps_altitude,omitempty"`           // GPS altitude
	GPSAccuracy          *float64 `json:"gps_accuracy,omitempty"`           // GPS accuracy in meters
	GPSSource            *string  `json:"gps_source,omitempty"`             // GPS source (rutos/starlink)
	GPSUncertaintyMeters *float64 `json:"gps_uncertainty_meters,omitempty"` // GPS uncertainty
	GPSTimeS             *int64   `json:"gps_time_s,omitempty"`             // GPS time in seconds
	GPSTimeUTCOffsetS    *int64   `json:"gps_time_utc_offset_s,omitempty"`  // UTC offset in seconds

	// Enhanced Cellular Stability Metrics
	StabilityScore  *float64 `json:"stability_score,omitempty"`  // 0-100 stability score
	StabilityStatus *string  `json:"stability_status,omitempty"` // "healthy", "degraded", "unhealthy"
	PredictiveRisk  *float64 `json:"predictive_risk,omitempty"`  // 0-1 risk of impending failure
	ThroughputKbps  *float64 `json:"throughput_kbps,omitempty"`  // Calculated throughput in Kbps
	CellChanges     *int     `json:"cell_changes,omitempty"`     // Cell changes in monitoring window
	SignalVariance  *float64 `json:"signal_variance,omitempty"`  // Signal strength variance (stability indicator)

	// Predictive Obstruction Management
	PredictiveFailover *bool   `json:"predictive_failover,omitempty"` // True if predictive failover should be triggered
	PredictiveReason   *string `json:"predictive_reason,omitempty"`   // Reason for predictive failover
}

// Score represents the health scoring for a member
type Score struct {
	Instant   float64   `json:"instant"`
	EWMA      float64   `json:"ewma"`
	Final     float64   `json:"final"`
	UpdatedAt time.Time `json:"updated_at"`

	// Enhanced scoring breakdown
	QualityFactors map[string]float64 `json:"quality_factors,omitempty"` // Detailed quality breakdown
	Penalties      map[string]float64 `json:"penalties,omitempty"`       // Applied penalties
	Bonuses        map[string]float64 `json:"bonuses,omitempty"`         // Applied bonuses
}

// MemberState represents the current state of a member
type MemberState struct {
	Member  *Member  `json:"member"`
	Metrics *Metrics `json:"metrics,omitempty"`
	Score   *Score   `json:"score,omitempty"`
	Status  string   `json:"status"` // eligible|cooldown|warmup|failed
}

// Event represents a system event
type Event struct {
	ID        string                 `json:"id"`
	Type      string                 `json:"type"`
	Timestamp time.Time              `json:"timestamp"`
	Member    string                 `json:"member,omitempty"`
	From      string                 `json:"from,omitempty"`
	To        string                 `json:"to,omitempty"`
	Reason    string                 `json:"reason,omitempty"`
	Data      map[string]interface{} `json:"data,omitempty"`
}

// StarlinkEvent represents a Starlink-specific event for enhanced scoring and prediction
type StarlinkEvent struct {
	Type      string                 `json:"type"`               // Event type (network_outage, obstruction, thermal, etc.)
	Severity  string                 `json:"severity"`           // critical, warning, info
	Timestamp time.Time              `json:"timestamp"`          // When the event occurred
	Duration  *int64                 `json:"duration,omitempty"` // Duration in seconds (for ongoing events)
	Message   string                 `json:"message"`            // Human-readable description
	Data      map[string]interface{} `json:"data,omitempty"`     // Additional event-specific data
}

// Decision represents a failover decision with detailed reasoning
type Decision struct {
	ID              string                 `json:"id"`
	Timestamp       time.Time              `json:"timestamp"`
	Type            string                 `json:"type"` // evaluation|soft_failover|hard_failover|restore|maintenance
	Member          string                 `json:"member,omitempty"`
	From            string                 `json:"from,omitempty"`
	To              string                 `json:"to,omitempty"`
	Reason          string                 `json:"reason,omitempty"`
	TriggerReason   string                 `json:"trigger_reason,omitempty"`
	QualityFactors  map[string]float64     `json:"quality_factors,omitempty"`
	Context         map[string]interface{} `json:"context,omitempty"`
	Success         bool                   `json:"success"`
	Error           string                 `json:"error,omitempty"`
	Duration        time.Duration          `json:"duration,omitempty"`
	Predictive      bool                   `json:"predictive"`
	LocationData    *GPSData               `json:"location_data,omitempty"`
	ObstructionData *ObstructionData       `json:"obstruction_data,omitempty"`
}

// GPSData represents GPS location information
type GPSData struct {
	Latitude          float64   `json:"latitude"`
	Longitude         float64   `json:"longitude"`
	Altitude          float64   `json:"altitude"`
	Accuracy          float64   `json:"accuracy"`
	Source            string    `json:"source"` // rutos|starlink|cellular
	Satellites        int       `json:"satellites"`
	Valid             bool      `json:"valid"`
	UncertaintyMeters float64   `json:"uncertainty_meters,omitempty"`
	Timestamp         time.Time `json:"timestamp"`
}

// ObstructionData represents detailed obstruction information
type ObstructionData struct {
	CurrentObstruction   float64 `json:"current_obstruction"`    // Current sky coverage obstruction
	TimeObstructed       float64 `json:"time_obstructed"`        // Historical time obstructed
	ValidDuration        int64   `json:"valid_duration"`         // Valid measurement duration
	AvgProlongedDuration float64 `json:"avg_prolonged_duration"` // Average prolonged obstruction duration
	PatchesValid         int     `json:"patches_valid"`          // Number of valid measurements
	DataQuality          string  `json:"data_quality"`           // good|poor|insufficient
	Assessment           string  `json:"assessment"`             // harmless|problematic|critical
}

// HealthStatus represents comprehensive health status
type HealthStatus struct {
	Overall         string            `json:"overall"`       // healthy|degraded|critical|unknown
	HardwareTest    string            `json:"hardware_test"` // PASSED|FAILED|UNKNOWN
	DLBWReason      string            `json:"dl_bw_reason"`  // NO_LIMIT|DATA_CAP|CONGESTION|etc
	ULBWReason      string            `json:"ul_bw_reason"`  // NO_LIMIT|DATA_CAP|CONGESTION|etc
	ThermalThrottle bool              `json:"thermal_throttle"`
	ThermalShutdown bool              `json:"thermal_shutdown"`
	Roaming         bool              `json:"roaming"`
	RebootImminent  bool              `json:"reboot_imminent"`
	RebootCountdown int64             `json:"reboot_countdown,omitempty"`
	Details         map[string]string `json:"details,omitempty"`
}

// LocationCluster represents a location-based performance cluster
type LocationCluster struct {
	CenterLatitude  float64   `json:"center_latitude"`
	CenterLongitude float64   `json:"center_longitude"`
	Radius          float64   `json:"radius"` // meters
	SampleCount     int       `json:"sample_count"`
	AvgLatency      float64   `json:"avg_latency"`
	AvgLoss         float64   `json:"avg_loss"`
	AvgObstruction  float64   `json:"avg_obstruction"`
	Problematic     bool      `json:"problematic"`
	FirstSeen       time.Time `json:"first_seen"`
	LastSeen        time.Time `json:"last_seen"`
}

// PredictiveData represents predictive analysis data
type PredictiveData struct {
	ObstructionSlope        float64       `json:"obstruction_slope"`        // Rate of obstruction change
	ObstructionAcceleration float64       `json:"obstruction_acceleration"` // Acceleration of obstruction change
	SNRTrend                float64       `json:"snr_trend"`                // SNR trend over time
	LatencyTrend            float64       `json:"latency_trend"`            // Latency trend over time
	LossTrend               float64       `json:"loss_trend"`               // Loss trend over time
	PredictionConfidence    float64       `json:"prediction_confidence"`    // 0-1 confidence in prediction
	PredictedIssue          string        `json:"predicted_issue"`          // Type of predicted issue
	TimeToIssue             time.Duration `json:"time_to_issue,omitempty"`  // Predicted time to issue
}

// AdaptiveSamplingConfig represents adaptive sampling configuration
type AdaptiveSamplingConfig struct {
	Enabled             bool           `json:"enabled"`
	FallBehindThreshold int            `json:"fall_behind_threshold"`
	SamplingInterval    int            `json:"sampling_interval"` // Process every Nth sample
	MaxSamplesPerRun    int            `json:"max_samples_per_run"`
	ConnectionTypeRules map[string]int `json:"connection_type_rules"` // Sampling intervals by connection type
}

// NotificationConfig represents notification configuration
type NotificationConfig struct {
	Enabled              bool           `json:"enabled"`
	PushoverEnabled      bool           `json:"pushover_enabled"`
	PushoverToken        string         `json:"pushover_token"`
	PushoverUser         string         `json:"pushover_user"`
	RateLimitPerHour     int            `json:"rate_limit_per_hour"`
	CooldownPeriod       time.Duration  `json:"cooldown_period"`
	PriorityLevels       map[string]int `json:"priority_levels"`
	EmergencyRetry       bool           `json:"emergency_retry"`
	EmergencyRetryPeriod time.Duration  `json:"emergency_retry_period"`
}

// Collector interface for collecting metrics from different member types
type Collector interface {
	Collect(ctx context.Context, member *Member) (*Metrics, error)
	Validate(member *Member) error
}

// Controller interface for controlling network interfaces
type Controller interface {
	Switch(from, to *Member) error
	GetCurrentMember() (*Member, error)
	Validate(member *Member) error
}

// DecisionLogger interface for logging decisions
type DecisionLogger interface {
	LogDecision(decision *Decision) error
	GetDecisions(since time.Time, limit int) ([]*Decision, error)
	GetDecisionStats(since time.Time) (map[string]interface{}, error)
}

// GPSCollector interface for collecting GPS data
type GPSCollector interface {
	CollectGPS(ctx context.Context) (*GPSData, error)
	ValidateGPS(gps *GPSData) error
	GetBestSource() string
}

// PredictiveEngine interface for predictive analysis
type PredictiveEngine interface {
	AnalyzePredictive(ctx context.Context, member *Member, metrics []*Metrics) (*PredictiveData, error)
	ShouldTriggerPredictiveFailover(predictive *PredictiveData, config map[string]interface{}) bool
}

// NotificationManager interface for sending notifications
type NotificationManager interface {
	SendNotification(title, message string, priority int, retry bool) error
	IsRateLimited() bool
	GetNotificationStats() map[string]interface{}
}

// InterfaceClass represents the type of network interface
type InterfaceClass string

// Member classes
const (
	ClassStarlink InterfaceClass = "starlink"
	ClassCellular InterfaceClass = "cellular"
	ClassWiFi     InterfaceClass = "wifi"
	ClassLAN      InterfaceClass = "lan"
	ClassOther    InterfaceClass = "other"
)

// Event types
const (
	EventMemberDiscovered  = "member_discovered"
	EventMemberLost        = "member_lost"
	EventFailover          = "failover"
	EventFailback          = "failback"
	EventPredictive        = "predictive"
	EventCooldown          = "cooldown"
	EventWarmup            = "warmup"
	EventError             = "error"
	EventConfigReload      = "config_reload"
	EventHealthCheck       = "health_check"
	EventSystemMaintenance = "system_maintenance"
	EventLocationChange    = "location_change"
	EventObstructionAlert  = "obstruction_alert"
)

// Decision types
const (
	DecisionEvaluation   = "evaluation"
	DecisionSoftFailover = "soft_failover"
	DecisionHardFailover = "hard_failover"
	DecisionRestore      = "restore"
	DecisionMaintenance  = "maintenance"
	DecisionPredictive   = "predictive"
)

// Member statuses
const (
	StatusEligible = "eligible"
	StatusCooldown = "cooldown"
	StatusWarmup   = "warmup"
	StatusFailed   = "failed"
)

// Detection modes
const (
	DetectAuto    = "auto"
	DetectDisable = "disable"
	DetectForce   = "force"
)

// Health status levels
const (
	HealthHealthy  = "healthy"
	HealthDegraded = "degraded"
	HealthCritical = "critical"
	HealthUnknown  = "unknown"
)

// GPS sources
const (
	GPSSourceRUTOS    = "rutos"
	GPSSourceStarlink = "starlink"
	GPSSourceCellular = "cellular"
)

// Connection types for adaptive sampling
const (
	ConnectionTypeUnlimited = "unlimited" // Starlink, LAN
	ConnectionTypeMetered   = "metered"   // Cellular
	ConnectionTypeLimited   = "limited"   // WiFi with data caps
)
