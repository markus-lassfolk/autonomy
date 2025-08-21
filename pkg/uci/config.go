package uci

import (
	"context"
	"fmt"
	"os"
	"strconv"
	"strings"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg"
)

// Config represents the autonomy configuration
type Config struct {
	// Main configuration
	Enable              bool   `json:"enable"`
	UseMWAN3            bool   `json:"use_mwan3"`
	PollIntervalMS      int    `json:"poll_interval_ms"`
	DecisionIntervalMS  int    `json:"decision_interval_ms"`
	DiscoveryIntervalMS int    `json:"discovery_interval_ms"`
	CleanupIntervalMS   int    `json:"cleanup_interval_ms"`
	HistoryWindowS      int    `json:"history_window_s"`
	RetentionHours      int    `json:"retention_hours"`
	MaxRAMMB            int    `json:"max_ram_mb"`
	DataCapMode         string `json:"data_cap_mode"`
	Predictive          bool   `json:"predictive"`
	SwitchMargin        int    `json:"switch_margin"`
	MinUptimeS          int    `json:"min_uptime_s"`
	CooldownS           int    `json:"cooldown_s"`
	MetricsListener     bool   `json:"metrics_listener"`
	HealthListener      bool   `json:"health_listener"`
	MetricsPort         int    `json:"metrics_port"`
	HealthPort          int    `json:"health_port"`
	LogLevel            string `json:"log_level"`
	LogFile             string `json:"log_file"`

	// Performance and Security
	PerformanceProfiling bool `json:"performance_profiling"`
	SecurityAuditing     bool `json:"security_auditing"`
	ProfilingEnabled     bool `json:"profiling_enabled"`
	AuditingEnabled      bool `json:"auditing_enabled"`

	// Machine Learning
	MLEnabled    bool   `json:"ml_enabled"`
	MLModelPath  string `json:"ml_model_path"`
	MLTraining   bool   `json:"ml_training"`
	MLPrediction bool   `json:"ml_prediction"`

	// Starlink API Configuration
	StarlinkAPIHost   string `json:"starlink_api_host"`
	StarlinkAPIPort   int    `json:"starlink_api_port"`
	StarlinkTimeout   int    `json:"starlink_timeout_s"`
	StarlinkGRPCFirst bool   `json:"starlink_grpc_first"`
	StarlinkHTTPFirst bool   `json:"starlink_http_first"`

	// Hybrid Weight System Configuration
	RespectUserWeights           bool `json:"respect_user_weights"`
	DynamicAdjustment            bool `json:"dynamic_adjustment"`
	EmergencyOverride            bool `json:"emergency_override"`
	OnlyEmergencyOverride        bool `json:"only_emergency_override"`
	RestoreTimeoutS              int  `json:"restore_timeout_s"`
	MinimalAdjustmentPoints      int  `json:"minimal_adjustment_points"`
	TemporaryBoostPoints         int  `json:"temporary_boost_points"`
	TemporaryAdjustmentDurationS int  `json:"temporary_adjustment_duration_s"`
	EmergencyAdjustmentDurationS int  `json:"emergency_adjustment_duration_s"`

	// Intelligent Adjustment Thresholds
	StarlinkObstructionThreshold float64 `json:"starlink_obstruction_threshold"`
	CellularSignalThreshold      float64 `json:"cellular_signal_threshold"`
	LatencyDegradationThreshold  float64 `json:"latency_degradation_threshold"`
	LossThreshold                float64 `json:"loss_threshold"`

	// Security Configuration
	AllowedIPs        []string `json:"allowed_ips"`
	BlockedIPs        []string `json:"blocked_ips"`
	AllowedPorts      []int    `json:"allowed_ports"`
	BlockedPorts      []int    `json:"blocked_ports"`
	MaxFailedAttempts int      `json:"max_failed_attempts"`
	BlockDuration     int      `json:"block_duration"`

	// Thresholds
	FailThresholdLoss       int `json:"fail_threshold_loss"`
	FailThresholdLatency    int `json:"fail_threshold_latency"`
	FailMinDurationS        int `json:"fail_min_duration_s"`
	RestoreThresholdLoss    int `json:"restore_threshold_loss"`
	RestoreThresholdLatency int `json:"restore_threshold_latency"`
	RestoreMinDurationS     int `json:"restore_min_duration_s"`

	// Notifications - General Settings
	PriorityThreshold      string `json:"priority_threshold"`
	AcknowledgmentTracking bool   `json:"acknowledgment_tracking"`
	LocationEnabled        bool   `json:"location_enabled"`
	RichContextEnabled     bool   `json:"rich_context_enabled"`
	NotifyOnFailover       bool   `json:"notify_on_failover"`
	NotifyOnFailback       bool   `json:"notify_on_failback"`
	NotifyOnMemberDown     bool   `json:"notify_on_member_down"`
	NotifyOnMemberUp       bool   `json:"notify_on_member_up"`
	NotifyOnPredictive     bool   `json:"notify_on_predictive"`
	NotifyOnCritical       bool   `json:"notify_on_critical"`
	NotifyOnRecovery       bool   `json:"notify_on_recovery"`
	NotificationCooldownS  int    `json:"notification_cooldown_s"`
	MaxNotificationsHour   int    `json:"max_notifications_hour"`
	PriorityFailover       int    `json:"priority_failover"`
	PriorityFailback       int    `json:"priority_failback"`
	PriorityMemberDown     int    `json:"priority_member_down"`
	PriorityMemberUp       int    `json:"priority_member_up"`
	PriorityPredictive     int    `json:"priority_predictive"`
	PriorityCritical       int    `json:"priority_critical"`
	PriorityRecovery       int    `json:"priority_recovery"`

	// Pushover Configuration
	PushoverEnabled bool   `json:"pushover_enabled"`
	PushoverToken   string `json:"pushover_token"`
	PushoverUser    string `json:"pushover_user"`
	PushoverDevice  string `json:"pushover_device"`

	// Email Configuration
	EmailEnabled     bool     `json:"email_enabled"`
	EmailSMTPHost    string   `json:"email_smtp_host"`
	EmailSMTPPort    int      `json:"email_smtp_port"`
	EmailUsername    string   `json:"email_username"`
	EmailPassword    string   `json:"email_password"`
	EmailFrom        string   `json:"email_from"`
	EmailTo          []string `json:"email_to"`
	EmailUseTLS      bool     `json:"email_use_tls"`
	EmailUseStartTLS bool     `json:"email_use_starttls"`

	// Slack Configuration
	SlackEnabled    bool   `json:"slack_enabled"`
	SlackWebhookURL string `json:"slack_webhook_url"`
	SlackChannel    string `json:"slack_channel"`
	SlackUsername   string `json:"slack_username"`
	SlackIconEmoji  string `json:"slack_icon_emoji"`
	SlackIconURL    string `json:"slack_icon_url"`

	// Discord Configuration
	DiscordEnabled    bool   `json:"discord_enabled"`
	DiscordWebhookURL string `json:"discord_webhook_url"`
	DiscordUsername   string `json:"discord_username"`
	DiscordAvatarURL  string `json:"discord_avatar_url"`

	// Telegram Configuration
	TelegramEnabled bool   `json:"telegram_enabled"`
	TelegramToken   string `json:"telegram_token"`
	TelegramChatID  string `json:"telegram_chat_id"`

	// Webhook Configuration
	WebhookEnabled        bool              `json:"webhook_enabled"`
	WebhookURL            string            `json:"webhook_url"`
	WebhookMethod         string            `json:"webhook_method"`
	WebhookContentType    string            `json:"webhook_content_type"`
	WebhookHeaders        map[string]string `json:"webhook_headers"`
	WebhookTemplate       string            `json:"webhook_template"`
	WebhookTemplateFormat string            `json:"webhook_template_format"`
	WebhookAuthType       string            `json:"webhook_auth_type"`
	WebhookAuthToken      string            `json:"webhook_auth_token"`
	WebhookAuthUsername   string            `json:"webhook_auth_username"`
	WebhookAuthPassword   string            `json:"webhook_auth_password"`
	WebhookAuthHeader     string            `json:"webhook_auth_header"`
	WebhookTimeout        int               `json:"webhook_timeout"`
	WebhookRetryAttempts  int               `json:"webhook_retry_attempts"`
	WebhookRetryDelay     int               `json:"webhook_retry_delay"`
	WebhookVerifySSL      bool              `json:"webhook_verify_ssl"`
	WebhookFollowRedirect bool              `json:"webhook_follow_redirect"`
	WebhookPriorityFilter []int             `json:"webhook_priority_filter"`
	WebhookTypeFilter     []string          `json:"webhook_type_filter"`
	WebhookName           string            `json:"webhook_name"`
	WebhookDescription    string            `json:"webhook_description"`

	// Telemetry publish
	MQTTBroker string `json:"mqtt_broker"`
	MQTTTopic  string `json:"mqtt_topic"`

	// MQTT Configuration
	MQTT MQTTConfig `json:"mqtt"`

	// WiFi Optimization Configuration
	WiFiOptimizationEnabled    bool    `json:"wifi_optimization_enabled"`
	WiFiMovementThreshold      float64 `json:"wifi_movement_threshold"` // meters
	WiFiStationaryTime         int     `json:"wifi_stationary_time"`    // seconds
	WiFiNightlyOptimization    bool    `json:"wifi_nightly_optimization"`
	WiFiNightlyTime            string  `json:"wifi_nightly_time"`   // HH:MM format
	WiFiNightlyWindow          int     `json:"wifi_nightly_window"` // minutes
	WiFiWeeklyOptimization     bool    `json:"wifi_weekly_optimization"`
	WiFiWeeklyDays             string  `json:"wifi_weekly_days"`            // comma-separated
	WiFiWeeklyTime             string  `json:"wifi_weekly_time"`            // HH:MM format
	WiFiWeeklyWindow           int     `json:"wifi_weekly_window"`          // minutes
	WiFiMinImprovement         int     `json:"wifi_min_improvement"`        // minimum score improvement
	WiFiDwellTime              int     `json:"wifi_dwell_time"`             // seconds
	WiFiNoiseDefault           int     `json:"wifi_noise_default"`          // default noise floor
	WiFiVHT80Threshold         int     `json:"wifi_vht80_threshold"`        // threshold for VHT80
	WiFiVHT40Threshold         int     `json:"wifi_vht40_threshold"`        // threshold for VHT40
	WiFiUseDFS                 bool    `json:"wifi_use_dfs"`                // allow DFS channels
	WiFiOptimizationCooldown   int     `json:"wifi_optimization_cooldown"`  // seconds
	WiFiGPSAccuracyThreshold   float64 `json:"wifi_gps_accuracy_threshold"` // meters
	WiFiLocationLogging        bool    `json:"wifi_location_logging"`
	WiFiSchedulerCheckInterval int     `json:"wifi_scheduler_check_interval"` // minutes
	WiFiSkipIfRecent           bool    `json:"wifi_skip_if_recent"`
	WiFiRecentThreshold        int     `json:"wifi_recent_threshold"` // hours
	WiFiTimezone               string  `json:"wifi_timezone"`

	// Enhanced WiFi Scanning Configuration
	WiFiUseEnhancedScanner  bool    `json:"wifi_use_enhanced_scanner"`  // Use RUTOS-native enhanced scanning
	WiFiStrongRSSIThreshold int     `json:"wifi_strong_rssi_threshold"` // Strong interferer threshold (-60dBm)
	WiFiWeakRSSIThreshold   int     `json:"wifi_weak_rssi_threshold"`   // Weak interferer threshold (-80dBm)
	WiFiUtilizationWeight   int     `json:"wifi_utilization_weight"`    // Channel utilization penalty weight
	WiFiExcellentThreshold  int     `json:"wifi_excellent_threshold"`   // 5-star threshold (90)
	WiFiGoodThreshold       int     `json:"wifi_good_threshold"`        // 4-star threshold (75)
	WiFiFairThreshold       int     `json:"wifi_fair_threshold"`        // 3-star threshold (50)
	WiFiPoorThreshold       int     `json:"wifi_poor_threshold"`        // 2-star threshold (25)
	WiFiOverlapPenaltyRatio float64 `json:"wifi_overlap_penalty_ratio"` // Overlap penalty ratio (0.5)

	// Metered Mode Configuration
	MeteredModeEnabled           bool   `json:"metered_mode_enabled"`
	DataLimitWarningThreshold    int    `json:"data_limit_warning_threshold"`
	DataLimitCriticalThreshold   int    `json:"data_limit_critical_threshold"`
	DataUsageHysteresisMargin    int    `json:"data_usage_hysteresis_margin"`
	MeteredStabilityDelay        int    `json:"metered_stability_delay"`
	MeteredClientReconnectMethod string `json:"metered_client_reconnect_method"`
	MeteredModeDebug             bool   `json:"metered_mode_debug"`

	// GPS Configuration
	GPSEnabled                   bool     `json:"gps_enabled"`
	GPSSourcePriority            []string `json:"gps_source_priority"`       // ["rutos", "starlink", "cellular"]
	GPSMovementThresholdM        float64  `json:"gps_movement_threshold_m"`  // Movement detection threshold in meters
	GPSAccuracyThresholdM        float64  `json:"gps_accuracy_threshold_m"`  // Minimum accuracy required
	GPSStalenessThresholdS       int64    `json:"gps_staleness_threshold_s"` // Maximum age for GPS data
	GPSCollectionIntervalS       int      `json:"gps_collection_interval_s"` // Collection interval
	GPSMovementDetection         bool     `json:"gps_movement_detection"`    // Enable movement detection
	GPSLocationClustering        bool     `json:"gps_location_clustering"`   // Enable location clustering
	GPSRetryAttempts             int      `json:"gps_retry_attempts"`        // Number of retry attempts
	GPSRetryDelayS               int      `json:"gps_retry_delay_s"`         // Delay between retries
	GPSGoogleAPIEnabled          bool     `json:"gps_google_api_enabled"`
	GPSGoogleAPIKey              string   `json:"gps_google_api_key"`
	GPSGoogleElevationAPIEnabled bool     `json:"gps_google_elevation_api_enabled"` // Enable Google Maps Elevation API

	// Hybrid Confidence-Based Prioritization
	GPSHybridPrioritization        bool    `json:"gps_hybrid_prioritization"`         // Enable confidence-based fallback
	GPSMinAcceptableConfidence     float64 `json:"gps_min_acceptable_confidence"`     // Minimum confidence to accept (0.0-1.0)
	GPSFallbackConfidenceThreshold float64 `json:"gps_fallback_confidence_threshold"` // Threshold to try next source (0.0-1.0)

	// GPS API Server Configuration
	GPSAPIServerEnabled bool   `json:"gps_api_server_enabled"`
	GPSAPIServerPort    int    `json:"gps_api_server_port"`
	GPSAPIServerHost    string `json:"gps_api_server_host"`
	GPSAPIServerAuthKey string `json:"gps_api_server_auth_key"` // Optional authentication key

	// Cell Tower Location Configuration
	GPSCellTowerEnabled     bool   `json:"gps_cell_tower_enabled"`    // Enable cell tower location
	GPSMozillaEnabled       bool   `json:"gps_mozilla_enabled"`       // Enable Mozilla Location Service
	GPSOpenCellIDEnabled    bool   `json:"gps_opencellid_enabled"`    // Enable OpenCellID API
	GPSOpenCellIDAPIKey     string `json:"gps_opencellid_api_key"`    // OpenCellID API key
	GPSOpenCellIDContribute bool   `json:"gps_opencellid_contribute"` // Enable data contribution
	GPSCellTowerMaxCells    int    `json:"gps_cell_tower_max_cells"`  // Max cells for triangulation
	GPSCellTowerTimeout     int    `json:"gps_cell_tower_timeout"`    // Timeout in seconds

	// Enhanced OpenCellID Configuration
	GPSOpenCellIDCacheSizeMB           int     `json:"gps_opencellid_cache_size_mb"`            // Cache size in MB
	GPSOpenCellIDMaxCellsPerLookup     int     `json:"gps_opencellid_max_cells_per_lookup"`     // Max cells per API lookup
	GPSOpenCellIDNegativeCacheTTLHours int     `json:"gps_opencellid_negative_cache_ttl_hours"` // Negative cache TTL in hours
	GPSOpenCellIDContributionInterval  int     `json:"gps_opencellid_contribution_interval"`    // Contribution interval in minutes
	GPSOpenCellIDMinGPSAccuracy        float64 `json:"gps_opencellid_min_gps_accuracy"`         // Min GPS accuracy for contribution (meters)
	GPSOpenCellIDMovementThreshold     float64 `json:"gps_opencellid_movement_threshold"`       // Movement threshold for contribution (meters)
	GPSOpenCellIDRSRPChangeThreshold   float64 `json:"gps_opencellid_rsrp_change_threshold"`    // RSRP change threshold for contribution (dB)
	GPSOpenCellIDTimingAdvanceEnabled  bool    `json:"gps_opencellid_timing_advance_enabled"`   // Enable timing advance constraints
	GPSOpenCellIDFusionConfidence      float64 `json:"gps_opencellid_fusion_confidence"`        // Minimum fusion confidence threshold
	GPSOpenCellIDRatioLimit            float64 `json:"gps_opencellid_ratio_limit"`              // Max lookup:submission ratio (default: 8.0)
	GPSOpenCellIDRatioWindowHours      int     `json:"gps_opencellid_ratio_window_hours"`       // Rolling window for ratio calculation (default: 48)
	GPSOpenCellIDSchedulerEnabled      bool    `json:"gps_opencellid_scheduler_enabled"`        // Enable automated scheduling (default: true)
	GPSOpenCellIDMovingInterval        int     `json:"gps_opencellid_moving_interval"`          // Scan interval when moving in minutes (default: 2)
	GPSOpenCellIDStationaryInterval    int     `json:"gps_opencellid_stationary_interval"`      // Scan interval when stationary in minutes (default: 10)
	GPSOpenCellIDMaxScansPerHour       int     `json:"gps_opencellid_max_scans_per_hour"`       // Max scans per hour (default: 30)

	// Local Cell Database Configuration
	GPSLocalDBEnabled         bool    `json:"gps_local_db_enabled"`          // Enable local cell database
	GPSLocalDBPath            string  `json:"gps_local_db_path"`             // Database file path
	GPSLocalDBMaxObservations int     `json:"gps_local_db_max_observations"` // Max stored observations
	GPSLocalDBRetentionDays   int     `json:"gps_local_db_retention_days"`   // Data retention period
	GPSLocalDBMinAccuracy     float64 `json:"gps_local_db_min_accuracy"`     // Min accuracy to store

	// 5G Network Support Configuration
	GPS5GEnabled            bool `json:"gps_5g_enabled"`             // Enable 5G network collection
	GPS5GMaxNeighborCells   int  `json:"gps_5g_max_neighbor_cells"`  // Max 5G neighbor cells
	GPS5GSignalThreshold    int  `json:"gps_5g_signal_threshold"`    // Signal threshold in dBm
	GPS5GCarrierAggregation bool `json:"gps_5g_carrier_aggregation"` // Enable CA detection
	GPS5GCollectionTimeout  int  `json:"gps_5g_collection_timeout"`  // Collection timeout in seconds

	// Cellular Intelligence Configuration
	GPSCellularIntelEnabled    bool `json:"gps_cellular_intel_enabled"`    // Enable cellular intelligence
	GPSCellularMaxNeighbors    int  `json:"gps_cellular_max_neighbors"`    // Max neighbor cells to track
	GPSCellularSignalThreshold int  `json:"gps_cellular_signal_threshold"` // Signal threshold
	GPSCellularFingerprinting  bool `json:"gps_cellular_fingerprinting"`   // Enable fingerprinting

	// GPS Health Monitoring Configuration
	GPSHealthEnabled       bool    `json:"gps_health_enabled"`         // Enable health monitoring
	GPSHealthCheckInterval int     `json:"gps_health_check_interval"`  // Check interval in seconds
	GPSHealthMaxFailures   int     `json:"gps_health_max_failures"`    // Max consecutive failures
	GPSHealthMinAccuracy   float64 `json:"gps_health_min_accuracy"`    // Min acceptable accuracy
	GPSHealthMinSatellites int     `json:"gps_health_min_satellites"`  // Min satellites required
	GPSHealthMaxHDOP       float64 `json:"gps_health_max_hdop"`        // Max HDOP threshold
	GPSHealthAutoReset     bool    `json:"gps_health_auto_reset"`      // Enable auto reset
	GPSHealthResetCooldown int     `json:"gps_health_reset_cooldown"`  // Reset cooldown in seconds
	GPSHealthNotifyOnReset bool    `json:"gps_health_notify_on_reset"` // Send notifications

	// Adaptive Cache Configuration
	GPSAdaptiveCacheEnabled     bool    `json:"gps_adaptive_cache_enabled"`      // Enable adaptive caching
	GPSCacheMovementThreshold   float64 `json:"gps_cache_movement_threshold"`    // Movement detection threshold
	GPSCacheCellChangeThreshold float64 `json:"gps_cache_cell_change_threshold"` // Cell change threshold
	GPSCacheWiFiChangeThreshold float64 `json:"gps_cache_wifi_change_threshold"` // WiFi change threshold
	GPSCacheSoftTTL             int     `json:"gps_cache_soft_ttl"`              // Soft TTL in seconds
	GPSCacheHardTTL             int     `json:"gps_cache_hard_ttl"`              // Hard TTL in seconds
	GPSCacheMonthlyQuota        int     `json:"gps_cache_monthly_quota"`         // Monthly API quota

	// Adaptive Sampling Configuration
	AdaptiveSamplingEnabled             bool    `json:"adaptive_sampling_enabled"`
	AdaptiveSamplingBaseInterval        int     `json:"adaptive_sampling_base_interval"`
	AdaptiveSamplingMaxInterval         int     `json:"adaptive_sampling_max_interval"`
	AdaptiveSamplingMinInterval         int     `json:"adaptive_sampling_min_interval"`
	AdaptiveSamplingAdaptationRate      float64 `json:"adaptive_sampling_adaptation_rate"`
	AdaptiveSamplingFallBehindThreshold int     `json:"adaptive_sampling_fall_behind_threshold"`
	AdaptiveSamplingMaxSamplesPerRun    int     `json:"adaptive_sampling_max_samples_per_run"`
	AdaptiveSamplingStarlinkInterval    int     `json:"adaptive_sampling_starlink_interval"`
	AdaptiveSamplingCellularInterval    int     `json:"adaptive_sampling_cellular_interval"`
	AdaptiveSamplingWiFiInterval        int     `json:"adaptive_sampling_wifi_interval"`
	AdaptiveSamplingLANInterval         int     `json:"adaptive_sampling_lan_interval"`

	// Rate Optimizer Configuration
	RateOptimizerEnabled             bool    `json:"rate_optimizer_enabled"`
	RateOptimizerBaseInterval        int     `json:"rate_optimizer_base_interval"`
	RateOptimizerMaxInterval         int     `json:"rate_optimizer_max_interval"`
	RateOptimizerMinInterval         int     `json:"rate_optimizer_min_interval"`
	RateOptimizerFallBehindThreshold int     `json:"rate_optimizer_fall_behind_threshold"`
	RateOptimizerWindow              int     `json:"rate_optimizer_window"`
	RateOptimizerGradual             bool    `json:"rate_optimizer_gradual"`
	RateOptimizerPerformanceWeight   float64 `json:"rate_optimizer_performance_weight"`
	RateOptimizerDataUsageWeight     float64 `json:"rate_optimizer_data_usage_weight"`

	// Connection Detection Configuration
	ConnectionDetectionEnabled             bool     `json:"connection_detection_enabled"`
	ConnectionDetectionInterval            int      `json:"connection_detection_interval"`
	ConnectionDetectionStarlinkIPRange     string   `json:"connection_detection_starlink_ip_range"`
	ConnectionDetectionStarlinkGateway     string   `json:"connection_detection_starlink_gateway"`
	ConnectionDetectionCellularInterfaces  []string `json:"connection_detection_cellular_interfaces"`
	ConnectionDetectionWiFiInterfaces      []string `json:"connection_detection_wifi_interfaces"`
	ConnectionDetectionLANInterfaces       []string `json:"connection_detection_lan_interfaces"`
	ConnectionDetectionTimeout             int      `json:"connection_detection_timeout"`
	ConnectionDetectionConfidenceThreshold float64  `json:"connection_detection_confidence_threshold"`
	ConnectionDetectionMaxHistorySize      int      `json:"connection_detection_max_history_size"`

	// Member configurations
	Members map[string]*MemberConfig `json:"members"`

	// Internal state (none currently needed)
}

// MQTTConfig represents MQTT configuration
type MQTTConfig struct {
	Broker      string `json:"broker"`
	Port        int    `json:"port"`
	ClientID    string `json:"client_id"`
	Username    string `json:"username"`
	Password    string `json:"password"`
	TopicPrefix string `json:"topic_prefix"`
	QoS         int    `json:"qos"`
	Retain      bool   `json:"retain"`
	Enabled     bool   `json:"enabled"`
}

// MemberConfig represents configuration for a specific member
type MemberConfig struct {
	Detect        string `json:"detect"`
	Class         string `json:"class"`
	Weight        int    `json:"weight"`
	MinUptimeS    int    `json:"min_uptime_s"`
	CooldownS     int    `json:"cooldown_s"`
	PreferRoaming bool   `json:"prefer_roaming"`
	Metered       bool   `json:"metered"`
}

// Default configuration values
const (
	DefaultPollIntervalMS          = 1500
	DefaultHistoryWindowS          = 600
	DefaultRetentionHours          = 24
	DefaultMaxRAMMB                = 16
	DefaultDataCapMode             = "balanced"
	DefaultSwitchMargin            = 10
	DefaultMinUptimeS              = 5
	DefaultCooldownS               = 20
	DefaultLogLevel                = "info"
	DefaultFailThresholdLoss       = 5
	DefaultFailThresholdLatency    = 1200
	DefaultFailMinDurationS        = 10
	DefaultRestoreThresholdLoss    = 1
	DefaultRestoreThresholdLatency = 800
	DefaultRestoreMinDurationS     = 30
)

// LoadConfig loads and validates the autonomy configuration from UCI
func LoadConfig(path string) (*Config, error) {
	// If a specific file path is provided, use file-based loading directly
	// This is important for testing and when we want to use structured configs
	if path != "" && path != "/etc/config/autonomy" {
		return loadConfigFromFile(path)
	}

	// Try to load from UCI first for production use
	uci := NewUCI(nil) // We'll create a proper logger later
	config, err := uci.LoadConfig(context.Background())
	if err != nil {
		// Fallback to file-based loading for development/testing
		return loadConfigFromFile(path)
	}

	// Validate configuration
	if err := config.validate(); err != nil {
		return nil, fmt.Errorf("configuration validation failed: %w", err)
	}

	return config, nil
}

// loadConfigFromFile loads configuration from a file (fallback method)
func loadConfigFromFile(path string) (*Config, error) {
	cfg := &Config{
		Members: make(map[string]*MemberConfig),
	}

	// Set defaults
	cfg.setDefaults()

	// Check if config file exists
	if _, err := os.Stat(path); os.IsNotExist(err) {
		// Return default config if file doesn't exist
		return cfg, nil
	}

	// Parse UCI configuration
	if err := cfg.parseUCI(path); err != nil {
		return nil, fmt.Errorf("failed to parse UCI config: %w", err)
	}

	// Validate configuration
	if err := cfg.validate(); err != nil {
		return nil, fmt.Errorf("configuration validation failed: %w", err)
	}

	return cfg, nil
}

// setDefaults sets default values for the configuration
func (c *Config) setDefaults() {
	c.Enable = true
	c.UseMWAN3 = true
	c.PollIntervalMS = DefaultPollIntervalMS
	c.DecisionIntervalMS = 5000
	c.DiscoveryIntervalMS = 30000
	c.CleanupIntervalMS = 60000
	c.HistoryWindowS = DefaultHistoryWindowS
	c.RetentionHours = DefaultRetentionHours
	c.MaxRAMMB = DefaultMaxRAMMB
	c.DataCapMode = DefaultDataCapMode
	c.Predictive = true
	c.SwitchMargin = DefaultSwitchMargin
	c.MinUptimeS = DefaultMinUptimeS
	c.CooldownS = DefaultCooldownS
	c.MetricsListener = false
	c.HealthListener = true
	c.MetricsPort = 9090
	c.HealthPort = 8080
	c.LogLevel = DefaultLogLevel
	c.LogFile = ""

	// Performance and Security defaults
	c.PerformanceProfiling = false
	c.SecurityAuditing = false
	c.ProfilingEnabled = false
	c.AuditingEnabled = false

	// Machine Learning defaults
	c.MLEnabled = false
	c.MLModelPath = "/tmp/autonomy/models"
	c.MLTraining = false
	c.MLPrediction = false

	// Starlink API defaults
	c.StarlinkAPIHost = "192.168.100.1"
	c.StarlinkAPIPort = 9200
	c.StarlinkTimeout = 10
	c.StarlinkGRPCFirst = true
	c.StarlinkHTTPFirst = false

	// Hybrid Weight System defaults
	c.RespectUserWeights = true
	c.DynamicAdjustment = true
	c.EmergencyOverride = true
	c.OnlyEmergencyOverride = true
	c.RestoreTimeoutS = 300 // 5 minutes
	c.MinimalAdjustmentPoints = 10
	c.TemporaryBoostPoints = 20
	c.TemporaryAdjustmentDurationS = 300 // 5 minutes
	c.EmergencyAdjustmentDurationS = 900 // 15 minutes

	// Intelligent Adjustment Thresholds defaults
	c.StarlinkObstructionThreshold = 10.0 // 10% obstruction
	c.CellularSignalThreshold = -110.0    // dBm
	c.LatencyDegradationThreshold = 500.0 // ms
	c.LossThreshold = 5.0                 // 5% packet loss

	// Security defaults
	c.AllowedIPs = []string{}
	c.BlockedIPs = []string{}
	c.AllowedPorts = []int{8080, 9090}
	c.BlockedPorts = []int{22, 23, 25}
	c.MaxFailedAttempts = 5
	c.BlockDuration = 24

	c.FailThresholdLoss = DefaultFailThresholdLoss
	c.FailThresholdLatency = DefaultFailThresholdLatency
	c.FailMinDurationS = DefaultFailMinDurationS
	c.RestoreThresholdLoss = DefaultRestoreThresholdLoss
	c.RestoreThresholdLatency = DefaultRestoreThresholdLatency
	c.RestoreMinDurationS = DefaultRestoreMinDurationS

	// Notification defaults
	c.PushoverToken = ""
	c.PushoverUser = ""
	c.PushoverEnabled = false
	c.PushoverDevice = ""
	c.PriorityThreshold = "warning"
	c.AcknowledgmentTracking = true
	c.LocationEnabled = true
	c.RichContextEnabled = true
	c.NotifyOnFailover = true
	c.NotifyOnFailback = true
	c.NotifyOnMemberDown = true
	c.NotifyOnMemberUp = false
	c.NotifyOnPredictive = true
	c.NotifyOnCritical = true
	c.NotifyOnRecovery = true
	c.NotificationCooldownS = 300 // 5 minutes
	c.MaxNotificationsHour = 20
	c.PriorityFailover = 1   // High
	c.PriorityFailback = 0   // Normal
	c.PriorityMemberDown = 1 // High
	c.PriorityMemberUp = -1  // Low
	c.PriorityPredictive = 0 // Normal
	c.PriorityCritical = 2   // Emergency
	c.PriorityRecovery = 0   // Normal

	c.MQTTBroker = ""
	c.MQTTTopic = "autonomy/status"

	// Set MQTT defaults
	c.MQTT = MQTTConfig{
		Broker:      "localhost",
		Port:        1883,
		ClientID:    "autonomyd",
		TopicPrefix: "autonomy",
		QoS:         1,
		Retain:      false,
		Enabled:     false,
	}

	// WiFi Optimization defaults
	c.WiFiOptimizationEnabled = false // Disabled by default
	c.WiFiMovementThreshold = 100.0   // 100 meters (more sensitive than main GPS 500m)
	c.WiFiStationaryTime = 1800       // 30 minutes (same as GPS default)
	c.WiFiNightlyOptimization = true  // Enable nightly optimization
	c.WiFiNightlyTime = "03:00"       // 3 AM
	c.WiFiNightlyWindow = 30          // 30 minute window
	c.WiFiWeeklyOptimization = false  // Disabled by default
	c.WiFiWeeklyDays = "sunday"       // Sunday only
	c.WiFiWeeklyTime = "02:00"        // 2 AM
	c.WiFiWeeklyWindow = 60           // 60 minute window
	c.WiFiMinImprovement = 15         // 15 point minimum improvement
	c.WiFiDwellTime = 30              // 30 seconds dwell time
	c.WiFiNoiseDefault = -95          // -95 dBm default noise floor
	c.WiFiVHT80Threshold = 80         // 80 point threshold for VHT80
	c.WiFiVHT40Threshold = 60         // 60 point threshold for VHT40
	c.WiFiUseDFS = true               // Allow DFS channels
	c.WiFiOptimizationCooldown = 7200 // 2 hours cooldown
	c.WiFiGPSAccuracyThreshold = 50.0 // 50 meters GPS accuracy
	c.WiFiLocationLogging = true      // Enable location logging
	c.WiFiSchedulerCheckInterval = 15 // 15 minutes check interval
	c.WiFiSkipIfRecent = true         // Skip if recent optimization
	c.WiFiRecentThreshold = 6         // 6 hours recent threshold
	c.WiFiTimezone = "UTC"            // UTC timezone

	// Enhanced WiFi Scanning defaults
	c.WiFiUseEnhancedScanner = true // Enable enhanced scanning by default
	c.WiFiStrongRSSIThreshold = -60 // Strong interferer threshold (-60dBm)
	c.WiFiWeakRSSIThreshold = -80   // Weak interferer threshold (-80dBm)
	c.WiFiUtilizationWeight = 100   // Full weight for utilization penalty
	c.WiFiExcellentThreshold = 90   // 5 stars threshold (90 points)
	c.WiFiGoodThreshold = 75        // 4 stars threshold (75 points)
	c.WiFiFairThreshold = 50        // 3 stars threshold (50 points)
	c.WiFiPoorThreshold = 25        // 2 stars threshold (25 points)
	c.WiFiOverlapPenaltyRatio = 0.5 // Overlap penalty is 50% of co-channel

	// Metered Mode defaults
	c.MeteredModeEnabled = false              // Disabled by default
	c.DataLimitWarningThreshold = 80          // 80% warning threshold
	c.DataLimitCriticalThreshold = 95         // 95% critical threshold
	c.DataUsageHysteresisMargin = 5           // 5% hysteresis margin
	c.MeteredStabilityDelay = 300             // 5 minutes stability delay
	c.MeteredClientReconnectMethod = "gentle" // Gentle reconnection by default
	c.MeteredModeDebug = false                // Debug disabled by default

	// GPS defaults
	c.GPSEnabled = true // Enabled by default
	c.GPSSourcePriority = []string{"rutos", "starlink", "cellular"}
	c.GPSMovementThresholdM = 500.0        // 500 meters movement threshold
	c.GPSAccuracyThresholdM = 50.0         // 50 meters accuracy threshold
	c.GPSStalenessThresholdS = 300         // 5 minutes staleness threshold
	c.GPSCollectionIntervalS = 60          // 1 minute collection interval
	c.GPSMovementDetection = true          // Enable movement detection
	c.GPSLocationClustering = true         // Enable location clustering
	c.GPSRetryAttempts = 3                 // Number of retry attempts
	c.GPSRetryDelayS = 5                   // Delay between retries
	c.GPSGoogleAPIEnabled = false          // Disabled by default
	c.GPSGoogleAPIKey = ""                 // Empty by default
	c.GPSGoogleElevationAPIEnabled = false // Enable Google Maps Elevation API

	// GPS API Server defaults
	c.GPSAPIServerEnabled = false
	c.GPSAPIServerPort = 8081
	c.GPSAPIServerHost = "localhost"
	c.GPSAPIServerAuthKey = ""

	// Cell Tower Location defaults
	c.GPSCellTowerEnabled = false    // Disabled by default
	c.GPSMozillaEnabled = true       // Mozilla Location Service enabled
	c.GPSOpenCellIDEnabled = false   // Requires API key
	c.GPSOpenCellIDAPIKey = ""       // Empty by default
	c.GPSOpenCellIDContribute = true // Enable contribution when API key is set
	c.GPSCellTowerMaxCells = 6       // Max 6 cells for triangulation
	c.GPSCellTowerTimeout = 30       // 30 seconds timeout

	// Enhanced OpenCellID defaults
	c.GPSOpenCellIDCacheSizeMB = 25            // 25 MB cache size
	c.GPSOpenCellIDMaxCellsPerLookup = 5       // Max 5 cells per lookup
	c.GPSOpenCellIDNegativeCacheTTLHours = 12  // 12 hour negative cache TTL
	c.GPSOpenCellIDContributionInterval = 10   // 10 minute contribution interval
	c.GPSOpenCellIDMinGPSAccuracy = 20.0       // 20 meter minimum GPS accuracy
	c.GPSOpenCellIDMovementThreshold = 250.0   // 250 meter movement threshold
	c.GPSOpenCellIDRSRPChangeThreshold = 6.0   // 6 dB RSRP change threshold
	c.GPSOpenCellIDTimingAdvanceEnabled = true // Enable timing advance
	c.GPSOpenCellIDFusionConfidence = 0.5      // 50% minimum fusion confidence
	c.GPSOpenCellIDRatioLimit = 8.0            // 8:1 lookup:submission ratio
	c.GPSOpenCellIDRatioWindowHours = 48       // 48-hour rolling window
	c.GPSOpenCellIDSchedulerEnabled = true     // Enable automated scheduling
	c.GPSOpenCellIDMovingInterval = 2          // 2 minutes when moving
	c.GPSOpenCellIDStationaryInterval = 10     // 10 minutes when stationary
	c.GPSOpenCellIDMaxScansPerHour = 30        // Max 30 scans per hour

	// Local Cell Database defaults
	c.GPSLocalDBEnabled = false                             // Disabled by default
	c.GPSLocalDBPath = "/tmp/autonomy_cell_observations.db" // Default path
	c.GPSLocalDBMaxObservations = 10000                     // Max 10k observations
	c.GPSLocalDBRetentionDays = 30                          // 30 days retention
	c.GPSLocalDBMinAccuracy = 100.0                         // 100m min accuracy

	// 5G Network Support defaults
	c.GPS5GEnabled = true            // Enabled by default
	c.GPS5GMaxNeighborCells = 8      // Max 8 neighbor cells
	c.GPS5GSignalThreshold = -120    // -120 dBm threshold
	c.GPS5GCarrierAggregation = true // Enable CA detection
	c.GPS5GCollectionTimeout = 10    // 10 seconds timeout

	// Cellular Intelligence defaults
	c.GPSCellularIntelEnabled = true    // Enabled by default
	c.GPSCellularMaxNeighbors = 8       // Max 8 neighbors
	c.GPSCellularSignalThreshold = -120 // -120 dBm threshold
	c.GPSCellularFingerprinting = true  // Enable fingerprinting

	// GPS Health Monitoring defaults
	c.GPSHealthEnabled = true       // Enabled by default
	c.GPSHealthCheckInterval = 300  // 5 minutes
	c.GPSHealthMaxFailures = 3      // 3 consecutive failures
	c.GPSHealthMinAccuracy = 10.0   // 10m min accuracy
	c.GPSHealthMinSatellites = 4    // 4 satellites minimum
	c.GPSHealthMaxHDOP = 5.0        // 5.0 max HDOP
	c.GPSHealthAutoReset = true     // Enable auto reset
	c.GPSHealthResetCooldown = 600  // 10 minutes cooldown
	c.GPSHealthNotifyOnReset = true // Send notifications

	// Adaptive Cache defaults
	c.GPSAdaptiveCacheEnabled = true     // Enabled by default
	c.GPSCacheMovementThreshold = 300.0  // 300m movement threshold
	c.GPSCacheCellChangeThreshold = 0.35 // 35% cell change threshold
	c.GPSCacheWiFiChangeThreshold = 0.40 // 40% WiFi change threshold
	c.GPSCacheSoftTTL = 900              // 15 minutes soft TTL
	c.GPSCacheHardTTL = 3600             // 60 minutes hard TTL
	c.GPSCacheMonthlyQuota = 10000       // 10k monthly quota

	// Adaptive Sampling defaults
	c.AdaptiveSamplingEnabled = true
	c.AdaptiveSamplingBaseInterval = 1
	c.AdaptiveSamplingMaxInterval = 120
	c.AdaptiveSamplingMinInterval = 1
	c.AdaptiveSamplingAdaptationRate = 0.2
	c.AdaptiveSamplingFallBehindThreshold = 10
	c.AdaptiveSamplingMaxSamplesPerRun = 5
	c.AdaptiveSamplingStarlinkInterval = 1
	c.AdaptiveSamplingCellularInterval = 30
	c.AdaptiveSamplingWiFiInterval = 10
	c.AdaptiveSamplingLANInterval = 5

	// Rate Optimizer defaults
	c.RateOptimizerEnabled = true
	c.RateOptimizerBaseInterval = 1
	c.RateOptimizerMaxInterval = 120
	c.RateOptimizerMinInterval = 1
	c.RateOptimizerFallBehindThreshold = 3
	c.RateOptimizerWindow = 300
	c.RateOptimizerGradual = true
	c.RateOptimizerPerformanceWeight = 0.6
	c.RateOptimizerDataUsageWeight = 0.4

	// Connection Detection defaults
	c.ConnectionDetectionEnabled = true
	c.ConnectionDetectionInterval = 30
	c.ConnectionDetectionStarlinkIPRange = "192.168.100.0/24"
	c.ConnectionDetectionStarlinkGateway = "192.168.100.1"
	c.ConnectionDetectionCellularInterfaces = []string{"wwan", "usb", "modem", "mobile"}
	c.ConnectionDetectionWiFiInterfaces = []string{"wlan", "wifi", "ath", "radio"}
	c.ConnectionDetectionLANInterfaces = []string{"eth", "lan", "ethernet"}
	c.ConnectionDetectionTimeout = 10
	c.ConnectionDetectionConfidenceThreshold = 0.7
	c.ConnectionDetectionMaxHistorySize = 100
}

// parseUCI parses the UCI configuration file
func (c *Config) parseUCI(path string) error {
	// Parse UCI configuration file using simple text parsing
	// This implements a basic UCI parser that handles both legacy and structured autonomy configuration formats

	data, err := os.ReadFile(path)
	if err != nil {
		return err
	}

	lines := strings.Split(string(data), "\n")
	var currentSectionType string
	var currentSectionName string

	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}

		if strings.HasPrefix(line, "config ") {
			parts := strings.Fields(line)
			if len(parts) >= 3 {
				currentSectionType = parts[1]
				currentSectionName = strings.Trim(parts[2], "'\"")

				// Handle member configuration sections
				if currentSectionType == "member" {
					if c.Members[currentSectionName] == nil {
						c.Members[currentSectionName] = &MemberConfig{
							Detect:     pkg.DetectAuto,
							Weight:     50,
							MinUptimeS: c.MinUptimeS,
							CooldownS:  c.CooldownS,
						}
					}
				}
			}
		} else if strings.HasPrefix(line, "option ") {
			parts := strings.Fields(line)
			if len(parts) >= 3 {
				optionName := parts[1]
				value := strings.Trim(parts[2], "'\"")

				// Route options to appropriate parsers based on section type and name
				c.parseOption(currentSectionType, currentSectionName, optionName, value)
			}
		}
	}

	return nil
}

// parseOption routes options to appropriate parsers based on section type and name
func (c *Config) parseOption(sectionType, sectionName, option, value string) {
	switch sectionType {
	case "autonomy":
		if sectionName == "main" {
			c.parseMainOption(option, value)
		}
	case "thresholds":
		c.parseThresholdsOption(sectionName, option, value)
	case "starlink":
		c.parseStarlinkOption(sectionName, option, value)
	case "ml":
		c.parseMLOption(sectionName, option, value)
	case "monitoring":
		c.parseMonitoringOption(sectionName, option, value)
	case "notifications":
		c.parseNotificationsOption(sectionName, option, value)
	case "wifi":
		c.parseWiFiOption(sectionName, option, value)
	case "metered":
		c.parseMeteredOption(sectionName, option, value)
	case "gps":
		c.parseGPSOption(sectionName, option, value)
	case "member":
		c.parseMemberOption(sectionName, option, value)
	default:
		// For backward compatibility, treat unknown sections as main options
		// This allows legacy single-section configs to continue working
		if sectionType == "autonomy" || sectionType == "" {
			c.parseMainOption(option, value)
		}
	}
}

// parseMainOption parses core daemon configuration options
func (c *Config) parseMainOption(option, value string) {
	switch option {
	// Core daemon control
	case "enable":
		c.Enable = value == "1"
	case "use_mwan3":
		c.UseMWAN3 = value == "1"
	case "log_level":
		if isValidLogLevel(value) {
			c.LogLevel = value
		}
	case "log_file":
		c.LogFile = value

	// Timing and intervals
	case "poll_interval_ms":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.PollIntervalMS = v
		}
	case "history_window_s":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.HistoryWindowS = v
		}
	case "min_uptime_s":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.MinUptimeS = v
		}
	case "cooldown_s":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.CooldownS = v
		}

	// Memory and storage
	case "retention_hours":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.RetentionHours = v
		}
	case "max_ram_mb":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.MaxRAMMB = v
		}

	// Decision making
	case "predictive":
		c.Predictive = value == "1"
	case "switch_margin":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.SwitchMargin = v
		}
	case "data_cap_mode":
		if isValidDataCapMode(value) {
			c.DataCapMode = value
		}

	// Service endpoints
	case "metrics_listener":
		c.MetricsListener = value == "1"
	case "health_listener":
		c.HealthListener = value == "1"

	// Performance and Security (keeping in main for now as they're system-wide)
	case "performance_profiling":
		c.PerformanceProfiling = value == "1"
	case "security_auditing":
		c.SecurityAuditing = value == "1"
	case "profiling_enabled":
		c.ProfilingEnabled = value == "1"
	case "auditing_enabled":
		c.AuditingEnabled = value == "1"
	case "max_failed_attempts":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.MaxFailedAttempts = v
		}
	case "block_duration":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.BlockDuration = v
		}

	// Legacy compatibility - handle options that might still be in main section
	// These will be moved to their respective sections in new configs
	case "ml_enabled":
		c.MLEnabled = value == "1"
	case "ml_model_path":
		c.MLModelPath = value
	case "ml_training":
		c.MLTraining = value == "1"
	case "ml_prediction":
		c.MLPrediction = value == "1"

	// Legacy compatibility - route old options to appropriate parsers
	// This allows existing configs to continue working while new configs use structured sections
	default:
		// Try to parse as section-specific option for backward compatibility
		c.parseLegacyOption(option, value)
	}
}

// parseLegacyOption handles legacy options that were previously in main section
func (c *Config) parseLegacyOption(option, value string) {
	// Route legacy options to their new section parsers
	switch {
	// Threshold options
	case strings.HasPrefix(option, "fail_") || strings.HasPrefix(option, "restore_"):
		c.parseThresholdsOption("legacy", option, value)
	// Starlink API options
	case strings.HasPrefix(option, "starlink_"):
		c.parseStarlinkOption("api", option, value)
	// Notification options
	case strings.HasPrefix(option, "pushover_") || strings.HasPrefix(option, "notify_") ||
		strings.HasPrefix(option, "priority_") || option == "notification_cooldown_s" ||
		option == "max_notifications_hour" || option == "acknowledgment_tracking" ||
		option == "location_enabled" || option == "rich_context_enabled":
		c.parseNotificationsOption("legacy", option, value)
	// MQTT options
	case strings.HasPrefix(option, "mqtt_"):
		c.parseMonitoringOption("mqtt", option, value)
	// WiFi options
	case strings.HasPrefix(option, "wifi_"):
		c.parseWiFiOption("optimization", option, value)
	// Metered mode options
	case strings.HasPrefix(option, "metered_") || strings.HasPrefix(option, "data_"):
		c.parseMeteredOption("settings", option, value)
	// Weight system options
	case strings.Contains(option, "weight") || strings.Contains(option, "adjustment") ||
		strings.Contains(option, "override") || strings.Contains(option, "boost"):
		c.parseThresholdsOption("weights", option, value)
	// Intelligence options
	case strings.Contains(option, "threshold") || strings.Contains(option, "signal"):
		c.parseThresholdsOption("intelligence", option, value)
	}
}

// parseThresholdsOption parses threshold configuration options
func (c *Config) parseThresholdsOption(sectionName, option, value string) {
	switch option {
	// Failover thresholds
	case "fail_threshold_loss":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.FailThresholdLoss = v
		}
	case "fail_threshold_latency":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.FailThresholdLatency = v
		}
	case "fail_min_duration_s":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.FailMinDurationS = v
		}
	// Restore thresholds
	case "restore_threshold_loss":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.RestoreThresholdLoss = v
		}
	case "restore_threshold_latency":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.RestoreThresholdLatency = v
		}
	case "restore_min_duration_s":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.RestoreMinDurationS = v
		}
	// Weight system options
	case "respect_user_weights":
		c.RespectUserWeights = value == "1"
	case "dynamic_adjustment":
		c.DynamicAdjustment = value == "1"
	case "emergency_override":
		c.EmergencyOverride = value == "1"
	case "only_emergency_override":
		c.OnlyEmergencyOverride = value == "1"
	case "restore_timeout_s":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.RestoreTimeoutS = v
		}
	case "minimal_adjustment_points":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.MinimalAdjustmentPoints = v
		}
	case "temporary_boost_points":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.TemporaryBoostPoints = v
		}
	case "temporary_adjustment_duration_s":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.TemporaryAdjustmentDurationS = v
		}
	case "emergency_adjustment_duration_s":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.EmergencyAdjustmentDurationS = v
		}
	// Intelligence thresholds
	case "starlink_obstruction_threshold":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 0 {
			c.StarlinkObstructionThreshold = v
		}
	case "cellular_signal_threshold":
		if v, err := strconv.ParseFloat(value, 64); err == nil {
			c.CellularSignalThreshold = v
		}
	case "latency_degradation_threshold":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v > 0 {
			c.LatencyDegradationThreshold = v
		}
	case "loss_threshold":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 0 {
			c.LossThreshold = v
		}
	}
}

// parseStarlinkOption parses Starlink API configuration options
func (c *Config) parseStarlinkOption(sectionName, option, value string) {
	switch option {
	case "starlink_api_host":
		if value != "" {
			c.StarlinkAPIHost = value
		}
	case "starlink_api_port":
		if v, err := strconv.Atoi(value); err == nil && v > 0 && v <= 65535 {
			c.StarlinkAPIPort = v
		}
	case "starlink_timeout_s":
		if v, err := strconv.Atoi(value); err == nil && v > 0 && v <= 300 {
			c.StarlinkTimeout = v
		}
	case "starlink_grpc_first":
		c.StarlinkGRPCFirst = value == "1"
	case "starlink_http_first":
		c.StarlinkHTTPFirst = value == "1"
	}
}

// parseMLOption parses Machine Learning configuration options
func (c *Config) parseMLOption(sectionName, option, value string) {
	switch option {
	case "ml_enabled", "enabled":
		c.MLEnabled = value == "1"
	case "ml_model_path", "model_path":
		c.MLModelPath = value
	case "ml_training", "training":
		c.MLTraining = value == "1"
	case "ml_prediction", "prediction":
		c.MLPrediction = value == "1"
	}
}

// parseMonitoringOption parses monitoring configuration options
func (c *Config) parseMonitoringOption(sectionName, option, value string) {
	switch option {
	case "mqtt_broker", "broker":
		c.MQTTBroker = value
	case "mqtt_topic", "topic":
		c.MQTTTopic = value
	}
}

// parseNotificationsOption parses notification configuration options
func (c *Config) parseNotificationsOption(sectionName, option, value string) {
	switch option {
	// Pushover credentials
	case "pushover_token", "token":
		c.PushoverToken = value
	case "pushover_user", "user":
		c.PushoverUser = value
	case "pushover_enabled", "enabled":
		c.PushoverEnabled = value == "1"
	case "pushover_device", "device":
		c.PushoverDevice = value
	// Notification behavior
	case "priority_threshold", "threshold":
		threshold := strings.ToLower(value)
		if threshold == "info" || threshold == "warning" || threshold == "critical" || threshold == "emergency" {
			c.PriorityThreshold = threshold
		}
	case "acknowledgment_tracking":
		c.AcknowledgmentTracking = value == "1"
	case "location_enabled":
		c.LocationEnabled = value == "1"
	case "rich_context_enabled":
		c.RichContextEnabled = value == "1"
	case "notification_cooldown_s", "cooldown_s":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.NotificationCooldownS = v
		}
	case "max_notifications_hour", "max_per_hour":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.MaxNotificationsHour = v
		}
	// Event types (support both old and new format)
	case "notify_on_failover", "failover", "event_failover":
		c.NotifyOnFailover = value == "1"
	case "notify_on_failback", "failback", "event_failback":
		c.NotifyOnFailback = value == "1"
	case "notify_on_member_down", "member_down", "event_member_down":
		c.NotifyOnMemberDown = value == "1"
	case "notify_on_member_up", "member_up", "event_member_up":
		c.NotifyOnMemberUp = value == "1"
	case "notify_on_predictive", "predictive", "event_predictive":
		c.NotifyOnPredictive = value == "1"
	case "notify_on_critical", "critical", "event_critical":
		c.NotifyOnCritical = value == "1"
	case "notify_on_recovery", "recovery", "event_recovery":
		c.NotifyOnRecovery = value == "1"
	// Priority levels
	case "priority_failover":
		if v, err := strconv.Atoi(value); err == nil && v >= -2 && v <= 2 {
			c.PriorityFailover = v
		}
	case "priority_failback":
		if v, err := strconv.Atoi(value); err == nil && v >= -2 && v <= 2 {
			c.PriorityFailback = v
		}
	case "priority_member_down":
		if v, err := strconv.Atoi(value); err == nil && v >= -2 && v <= 2 {
			c.PriorityMemberDown = v
		}
	case "priority_member_up":
		if v, err := strconv.Atoi(value); err == nil && v >= -2 && v <= 2 {
			c.PriorityMemberUp = v
		}
	case "priority_predictive":
		if v, err := strconv.Atoi(value); err == nil && v >= -2 && v <= 2 {
			c.PriorityPredictive = v
		}
	case "priority_critical":
		if v, err := strconv.Atoi(value); err == nil && v >= -2 && v <= 2 {
			c.PriorityCritical = v
		}
	case "priority_recovery":
		if v, err := strconv.Atoi(value); err == nil && v >= -2 && v <= 2 {
			c.PriorityRecovery = v
		}
	}
}

// parseWiFiOption parses WiFi optimization configuration options
func (c *Config) parseWiFiOption(sectionName, option, value string) {
	switch option {
	case "wifi_optimization_enabled", "enabled":
		c.WiFiOptimizationEnabled = value == "1"
	case "wifi_movement_threshold", "movement_threshold":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v > 0 {
			c.WiFiMovementThreshold = v
		}
	case "wifi_stationary_time", "stationary_time":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.WiFiStationaryTime = v
		}
	case "wifi_nightly_optimization", "nightly_enabled":
		c.WiFiNightlyOptimization = value == "1"
	case "wifi_nightly_time", "nightly_time":
		c.WiFiNightlyTime = value
	case "wifi_nightly_window", "nightly_window":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.WiFiNightlyWindow = v
		}
	case "wifi_weekly_optimization", "weekly_enabled":
		c.WiFiWeeklyOptimization = value == "1"
	case "wifi_weekly_days", "weekly_days":
		c.WiFiWeeklyDays = value
	case "wifi_weekly_time", "weekly_time":
		c.WiFiWeeklyTime = value
	case "wifi_weekly_window", "weekly_window":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.WiFiWeeklyWindow = v
		}
	case "wifi_min_improvement", "min_improvement":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.WiFiMinImprovement = v
		}
	case "wifi_dwell_time", "dwell_time":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.WiFiDwellTime = v
		}
	case "wifi_noise_default", "noise_default":
		if v, err := strconv.Atoi(value); err == nil {
			c.WiFiNoiseDefault = v
		}
	case "wifi_vht80_threshold", "vht80_threshold":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.WiFiVHT80Threshold = v
		}
	case "wifi_vht40_threshold", "vht40_threshold":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.WiFiVHT40Threshold = v
		}
	case "wifi_use_dfs", "use_dfs":
		c.WiFiUseDFS = value == "1"
	case "wifi_optimization_cooldown", "cooldown":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.WiFiOptimizationCooldown = v
		}
	case "wifi_gps_accuracy_threshold", "gps_accuracy_threshold":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v > 0 {
			c.WiFiGPSAccuracyThreshold = v
		}
	case "wifi_location_logging", "location_logging":
		c.WiFiLocationLogging = value == "1"
	case "wifi_scheduler_check_interval", "scheduler_check_interval":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.WiFiSchedulerCheckInterval = v
		}
	case "wifi_skip_if_recent", "skip_if_recent":
		c.WiFiSkipIfRecent = value == "1"
	case "wifi_recent_threshold", "recent_threshold":
		if v, err := strconv.Atoi(value); err == nil && v > 0 {
			c.WiFiRecentThreshold = v
		}

	// Enhanced WiFi Scanning Configuration
	case "wifi_use_enhanced_scanner", "use_enhanced_scanner":
		c.WiFiUseEnhancedScanner = value == "1"
	case "wifi_strong_rssi_threshold", "strong_rssi_threshold":
		if v, err := strconv.Atoi(value); err == nil && v >= -100 && v <= 0 {
			c.WiFiStrongRSSIThreshold = v
		}
	case "wifi_weak_rssi_threshold", "weak_rssi_threshold":
		if v, err := strconv.Atoi(value); err == nil && v >= -100 && v <= 0 {
			c.WiFiWeakRSSIThreshold = v
		}
	case "wifi_utilization_weight", "utilization_weight":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.WiFiUtilizationWeight = v
		}
	case "wifi_excellent_threshold", "excellent_threshold":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 && v <= 100 {
			c.WiFiExcellentThreshold = v
		}
	case "wifi_good_threshold", "good_threshold":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 && v <= 100 {
			c.WiFiGoodThreshold = v
		}
	case "wifi_fair_threshold", "fair_threshold":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 && v <= 100 {
			c.WiFiFairThreshold = v
		}
	case "wifi_poor_threshold", "poor_threshold":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 && v <= 100 {
			c.WiFiPoorThreshold = v
		}
	case "wifi_overlap_penalty_ratio", "overlap_penalty_ratio":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 0 && v <= 1 {
			c.WiFiOverlapPenaltyRatio = v
		}
	case "wifi_timezone", "timezone":
		c.WiFiTimezone = value
	}
}

// parseMeteredOption parses metered mode configuration options
func (c *Config) parseMeteredOption(sectionName, option, value string) {
	switch option {
	case "metered_mode_enabled", "enabled":
		c.MeteredModeEnabled = value == "1"
	case "data_limit_warning_threshold", "warning_threshold":
		if v, err := strconv.Atoi(value); err == nil && v > 0 && v <= 100 {
			c.DataLimitWarningThreshold = v
		}
	case "data_limit_critical_threshold", "critical_threshold":
		if v, err := strconv.Atoi(value); err == nil && v > 0 && v <= 100 {
			c.DataLimitCriticalThreshold = v
		}
	case "data_usage_hysteresis_margin", "hysteresis_margin":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 && v <= 50 {
			c.DataUsageHysteresisMargin = v
		}
	case "metered_stability_delay", "stability_delay":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.MeteredStabilityDelay = v
		}
	case "metered_client_reconnect_method", "reconnect_method":
		if value == "gentle" || value == "force" {
			c.MeteredClientReconnectMethod = value
		}
	case "metered_mode_debug", "debug":
		c.MeteredModeDebug = value == "1"
	}
}

// parseGPSOption parses GPS configuration options
func (c *Config) parseGPSOption(sectionName, option, value string) {
	switch option {
	case "enabled":
		c.GPSEnabled = value == "1"
	case "source_priority":
		// Parse comma-separated source priority list
		sources := strings.Split(value, ",")
		for i, source := range sources {
			sources[i] = strings.TrimSpace(source)
		}
		c.GPSSourcePriority = sources
	case "movement_threshold_m":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 0 {
			c.GPSMovementThresholdM = v
		}
	case "accuracy_threshold_m":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 0 {
			c.GPSAccuracyThresholdM = v
		}
	case "staleness_threshold_s":
		if v, err := strconv.ParseInt(value, 10, 64); err == nil && v >= 0 {
			c.GPSStalenessThresholdS = v
		}
	case "collection_interval_s":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.GPSCollectionIntervalS = v
		}
	case "movement_detection":
		c.GPSMovementDetection = value == "1"
	case "location_clustering":
		c.GPSLocationClustering = value == "1"
	case "retry_attempts":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.GPSRetryAttempts = v
		}
	case "retry_delay_s":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			c.GPSRetryDelayS = v
		}
	case "google_api_enabled":
		c.GPSGoogleAPIEnabled = value == "1"
	case "google_api_key":
		c.GPSGoogleAPIKey = value
	case "google_elevation_api_enabled":
		c.GPSGoogleElevationAPIEnabled = value == "1"
	case "hybrid_prioritization":
		c.GPSHybridPrioritization = value == "1"
	case "min_acceptable_confidence":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 0.0 && v <= 1.0 {
			c.GPSMinAcceptableConfidence = v
		}
	case "fallback_confidence_threshold":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 0.0 && v <= 1.0 {
			c.GPSFallbackConfidenceThreshold = v
		}
	case "api_server_enabled":
		c.GPSAPIServerEnabled = value == "1"
	case "api_server_port":
		if v, err := strconv.Atoi(value); err == nil && v >= 1 && v <= 65535 {
			c.GPSAPIServerPort = v
		}
	case "api_server_host":
		c.GPSAPIServerHost = value
	case "api_server_auth_key":
		c.GPSAPIServerAuthKey = value

	// Cell Tower Location options
	case "cell_tower_enabled":
		c.GPSCellTowerEnabled = value == "1"
	case "mozilla_enabled":
		c.GPSMozillaEnabled = value == "1"
	case "opencellid_enabled":
		c.GPSOpenCellIDEnabled = value == "1"
	case "opencellid_api_key":
		c.GPSOpenCellIDAPIKey = value
	case "opencellid_contribute":
		c.GPSOpenCellIDContribute = value == "1"
	case "opencellid_cache_size_mb":
		if v, err := strconv.Atoi(value); err == nil && v >= 5 && v <= 100 {
			c.GPSOpenCellIDCacheSizeMB = v
		}
	case "opencellid_max_cells_per_lookup":
		if v, err := strconv.Atoi(value); err == nil && v >= 1 && v <= 10 {
			c.GPSOpenCellIDMaxCellsPerLookup = v
		}
	case "opencellid_negative_cache_ttl_hours":
		if v, err := strconv.Atoi(value); err == nil && v >= 1 && v <= 168 {
			c.GPSOpenCellIDNegativeCacheTTLHours = v
		}
	case "opencellid_contribution_interval":
		if v, err := strconv.Atoi(value); err == nil && v >= 1 && v <= 60 {
			c.GPSOpenCellIDContributionInterval = v
		}
	case "opencellid_min_gps_accuracy":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 1.0 && v <= 100.0 {
			c.GPSOpenCellIDMinGPSAccuracy = v
		}
	case "opencellid_movement_threshold":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 10.0 && v <= 5000.0 {
			c.GPSOpenCellIDMovementThreshold = v
		}
	case "opencellid_rsrp_change_threshold":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 1.0 && v <= 20.0 {
			c.GPSOpenCellIDRSRPChangeThreshold = v
		}
	case "opencellid_timing_advance_enabled":
		c.GPSOpenCellIDTimingAdvanceEnabled = value == "1"
	case "opencellid_fusion_confidence":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 0.1 && v <= 1.0 {
			c.GPSOpenCellIDFusionConfidence = v
		}
	case "opencellid_ratio_limit":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 1.0 && v <= 50.0 {
			c.GPSOpenCellIDRatioLimit = v
		}
	case "opencellid_ratio_window_hours":
		if v, err := strconv.Atoi(value); err == nil && v >= 1 && v <= 168 { // 1 hour to 1 week
			c.GPSOpenCellIDRatioWindowHours = v
		}
	case "opencellid_scheduler_enabled":
		c.GPSOpenCellIDSchedulerEnabled = value == "1"
	case "opencellid_moving_interval":
		if v, err := strconv.Atoi(value); err == nil && v >= 1 && v <= 60 { // 1-60 minutes
			c.GPSOpenCellIDMovingInterval = v
		}
	case "opencellid_stationary_interval":
		if v, err := strconv.Atoi(value); err == nil && v >= 1 && v <= 120 { // 1-120 minutes
			c.GPSOpenCellIDStationaryInterval = v
		}
	case "opencellid_max_scans_per_hour":
		if v, err := strconv.Atoi(value); err == nil && v >= 1 && v <= 120 { // 1-120 scans per hour
			c.GPSOpenCellIDMaxScansPerHour = v
		}
	case "cell_tower_max_cells":
		if v, err := strconv.Atoi(value); err == nil && v >= 1 && v <= 20 {
			c.GPSCellTowerMaxCells = v
		}
	case "cell_tower_timeout":
		if v, err := strconv.Atoi(value); err == nil && v >= 5 && v <= 120 {
			c.GPSCellTowerTimeout = v
		}

	// Local Cell Database options
	case "local_db_enabled":
		c.GPSLocalDBEnabled = value == "1"
	case "local_db_path":
		c.GPSLocalDBPath = value
	case "local_db_max_observations":
		if v, err := strconv.Atoi(value); err == nil && v >= 100 {
			c.GPSLocalDBMaxObservations = v
		}
	case "local_db_retention_days":
		if v, err := strconv.Atoi(value); err == nil && v >= 1 {
			c.GPSLocalDBRetentionDays = v
		}
	case "local_db_min_accuracy":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 1.0 {
			c.GPSLocalDBMinAccuracy = v
		}

	// 5G Network Support options
	case "5g_enabled":
		c.GPS5GEnabled = value == "1"
	case "5g_max_neighbor_cells":
		if v, err := strconv.Atoi(value); err == nil && v >= 1 && v <= 20 {
			c.GPS5GMaxNeighborCells = v
		}
	case "5g_signal_threshold":
		if v, err := strconv.Atoi(value); err == nil && v >= -150 && v <= -50 {
			c.GPS5GSignalThreshold = v
		}
	case "5g_carrier_aggregation":
		c.GPS5GCarrierAggregation = value == "1"
	case "5g_collection_timeout":
		if v, err := strconv.Atoi(value); err == nil && v >= 5 && v <= 60 {
			c.GPS5GCollectionTimeout = v
		}

	// Cellular Intelligence options
	case "cellular_intel_enabled":
		c.GPSCellularIntelEnabled = value == "1"
	case "cellular_max_neighbors":
		if v, err := strconv.Atoi(value); err == nil && v >= 1 && v <= 20 {
			c.GPSCellularMaxNeighbors = v
		}
	case "cellular_signal_threshold":
		if v, err := strconv.Atoi(value); err == nil && v >= -150 && v <= -50 {
			c.GPSCellularSignalThreshold = v
		}
	case "cellular_fingerprinting":
		c.GPSCellularFingerprinting = value == "1"

	// GPS Health Monitoring options
	case "health_enabled":
		c.GPSHealthEnabled = value == "1"
	case "health_check_interval":
		if v, err := strconv.Atoi(value); err == nil && v >= 60 {
			c.GPSHealthCheckInterval = v
		}
	case "health_max_failures":
		if v, err := strconv.Atoi(value); err == nil && v >= 1 && v <= 10 {
			c.GPSHealthMaxFailures = v
		}
	case "health_min_accuracy":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 1.0 {
			c.GPSHealthMinAccuracy = v
		}
	case "health_min_satellites":
		if v, err := strconv.Atoi(value); err == nil && v >= 3 && v <= 20 {
			c.GPSHealthMinSatellites = v
		}
	case "health_max_hdop":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 1.0 && v <= 20.0 {
			c.GPSHealthMaxHDOP = v
		}
	case "health_auto_reset":
		c.GPSHealthAutoReset = value == "1"
	case "health_reset_cooldown":
		if v, err := strconv.Atoi(value); err == nil && v >= 60 {
			c.GPSHealthResetCooldown = v
		}
	case "health_notify_on_reset":
		c.GPSHealthNotifyOnReset = value == "1"

	// Adaptive Cache options
	case "adaptive_cache_enabled":
		c.GPSAdaptiveCacheEnabled = value == "1"
	case "cache_movement_threshold":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 10.0 {
			c.GPSCacheMovementThreshold = v
		}
	case "cache_cell_change_threshold":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 0.1 && v <= 1.0 {
			c.GPSCacheCellChangeThreshold = v
		}
	case "cache_wifi_change_threshold":
		if v, err := strconv.ParseFloat(value, 64); err == nil && v >= 0.1 && v <= 1.0 {
			c.GPSCacheWiFiChangeThreshold = v
		}
	case "cache_soft_ttl":
		if v, err := strconv.Atoi(value); err == nil && v >= 60 {
			c.GPSCacheSoftTTL = v
		}
	case "cache_hard_ttl":
		if v, err := strconv.Atoi(value); err == nil && v >= 300 {
			c.GPSCacheHardTTL = v
		}
	case "cache_monthly_quota":
		if v, err := strconv.Atoi(value); err == nil && v >= 100 {
			c.GPSCacheMonthlyQuota = v
		}
	}
}

// parseMemberOption parses a member configuration option
func (c *Config) parseMemberOption(memberName, option, value string) {
	member := c.Members[memberName]
	if member == nil {
		return
	}

	switch option {
	case "detect":
		if isValidDetectMode(value) {
			member.Detect = value
		}
	case "class":
		if isValidMemberClass(value) {
			member.Class = value
		}
	case "weight":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			member.Weight = v
		}
	case "min_uptime_s":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			member.MinUptimeS = v
		}
	case "cooldown_s":
		if v, err := strconv.Atoi(value); err == nil && v >= 0 {
			member.CooldownS = v
		}
	case "prefer_roaming":
		member.PreferRoaming = value == "1"
	case "metered":
		member.Metered = value == "1"
	}
}

// validate validates the configuration
func (c *Config) validate() error {
	if c.PollIntervalMS < 100 || c.PollIntervalMS > 10000 {
		return fmt.Errorf("poll_interval_ms must be between 100 and 10000")
	}

	if c.HistoryWindowS < 60 || c.HistoryWindowS > 3600 {
		return fmt.Errorf("history_window_s must be between 60 and 3600")
	}

	if c.RetentionHours < 1 || c.RetentionHours > 168 {
		return fmt.Errorf("retention_hours must be between 1 and 168")
	}

	if c.MaxRAMMB < 1 || c.MaxRAMMB > 128 {
		return fmt.Errorf("max_ram_mb must be between 1 and 128")
	}

	if c.SwitchMargin < 0 || c.SwitchMargin > 100 {
		return fmt.Errorf("switch_margin must be between 0 and 100")
	}

	return nil
}

// Helper functions for validation
func isValidDataCapMode(mode string) bool {
	validModes := []string{"balanced", "conservative", "aggressive"}
	for _, valid := range validModes {
		if mode == valid {
			return true
		}
	}
	return false
}

func isValidLogLevel(level string) bool {
	validLevels := []string{"debug", "info", "warn", "error"}
	for _, valid := range validLevels {
		if level == valid {
			return true
		}
	}
	return false
}

func isValidDetectMode(mode string) bool {
	validModes := []string{pkg.DetectAuto, pkg.DetectDisable, pkg.DetectForce}
	for _, valid := range validModes {
		if mode == valid {
			return true
		}
	}
	return false
}

func isValidMemberClass(class string) bool {
	validClasses := []string{string(pkg.ClassStarlink), string(pkg.ClassCellular), string(pkg.ClassWiFi), string(pkg.ClassLAN), string(pkg.ClassOther)}
	for _, valid := range validClasses {
		if class == valid {
			return true
		}
	}
	return false
}

// ConfigProvider interface methods for metered manager
func (c *Config) GetMeteredConfig() map[string]interface{} {
	return map[string]interface{}{
		"enabled":                         c.MeteredModeEnabled,
		"data_limit_warning_threshold":    c.DataLimitWarningThreshold,
		"data_limit_critical_threshold":   c.DataLimitCriticalThreshold,
		"data_usage_hysteresis_margin":    c.DataUsageHysteresisMargin,
		"metered_stability_delay":         c.MeteredStabilityDelay,
		"metered_client_reconnect_method": c.MeteredClientReconnectMethod,
		"metered_mode_debug":              c.MeteredModeDebug,
	}
}

func (c *Config) GetMeteredModeEnabled() bool {
	return c.MeteredModeEnabled
}

func (c *Config) GetDataLimitWarningThreshold() int {
	return c.DataLimitWarningThreshold
}

func (c *Config) GetDataLimitCriticalThreshold() int {
	return c.DataLimitCriticalThreshold
}

func (c *Config) GetDataUsageHysteresisMargin() int {
	return c.DataUsageHysteresisMargin
}

func (c *Config) GetMeteredStabilityDelay() int {
	return c.MeteredStabilityDelay
}

func (c *Config) GetMeteredClientReconnectMethod() string {
	return c.MeteredClientReconnectMethod
}

func (c *Config) GetMeteredModeDebug() bool {
	return c.MeteredModeDebug
}

// UCIConfigProvider interface methods for notifications
func (c *Config) GetPushoverConfig() (enabled bool, token, user, device string) {
	return c.PushoverEnabled, c.PushoverToken, c.PushoverUser, c.PushoverDevice
}

func (c *Config) GetEmailConfig() (enabled bool, smtpHost string, smtpPort int, username, password, from string, to []string, useTLS, useStartTLS bool) {
	return c.EmailEnabled, c.EmailSMTPHost, c.EmailSMTPPort, c.EmailUsername, c.EmailPassword, c.EmailFrom, c.EmailTo, c.EmailUseTLS, c.EmailUseStartTLS
}

func (c *Config) GetSlackConfig() (enabled bool, webhookURL, channel, username, iconEmoji, iconURL string) {
	return c.SlackEnabled, c.SlackWebhookURL, c.SlackChannel, c.SlackUsername, c.SlackIconEmoji, c.SlackIconURL
}

func (c *Config) GetDiscordConfig() (enabled bool, webhookURL, username, avatarURL string) {
	return c.DiscordEnabled, c.DiscordWebhookURL, c.DiscordUsername, c.DiscordAvatarURL
}

func (c *Config) GetTelegramConfig() (enabled bool, token, chatID string) {
	return c.TelegramEnabled, c.TelegramToken, c.TelegramChatID
}

func (c *Config) GetWebhookConfig() (enabled bool, url, method, contentType string, headers map[string]string, template, templateFormat, authType, authToken, authUsername, authPassword, authHeader string, timeout time.Duration, retryAttempts int, retryDelay time.Duration, verifySSL bool) {
	return c.WebhookEnabled, c.WebhookURL, c.WebhookMethod, c.WebhookContentType, c.WebhookHeaders, c.WebhookTemplate, c.WebhookTemplateFormat, c.WebhookAuthType, c.WebhookAuthToken, c.WebhookAuthUsername, c.WebhookAuthPassword, c.WebhookAuthHeader, time.Duration(c.WebhookTimeout) * time.Second, c.WebhookRetryAttempts, time.Duration(c.WebhookRetryDelay) * time.Second, c.WebhookVerifySSL
}

func (c *Config) GetSMSConfig() (enabled bool, provider, apiKey, from, to string, template string) {
	// SMS configuration not implemented in UCI config yet
	return false, "", "", "", "", ""
}
