package logx

import (
	"io"
	"os"
	"time"

	"github.com/sirupsen/logrus"
)

// Logger provides structured JSON logging
type Logger struct {
	logger *logrus.Logger
}

// NewLogger creates a new structured logger
func NewLogger(level string, component string) *Logger {
	logger := logrus.New()
	logger.SetFormatter(&logrus.JSONFormatter{
		TimestampFormat: time.RFC3339,
		FieldMap: logrus.FieldMap{
			logrus.FieldKeyTime:  "ts",
			logrus.FieldKeyLevel: "level",
			logrus.FieldKeyMsg:   "msg",
		},
	})
	logger.SetOutput(os.Stdout)

	// Set log level
	switch level {
	case "debug":
		logger.SetLevel(logrus.DebugLevel)
	case "info":
		logger.SetLevel(logrus.InfoLevel)
	case "warn":
		logger.SetLevel(logrus.WarnLevel)
	case "error":
		logger.SetLevel(logrus.ErrorLevel)
	case "trace":
		logger.SetLevel(logrus.TraceLevel)
	default:
		logger.SetLevel(logrus.InfoLevel)
	}

	result := &Logger{logger: logger}

	// Add component field if specified
	if component != "" {
		result = result.WithField("component", component)
	}

	return result
}

// SetLevel sets the logging level
func (l *Logger) SetLevel(level string) {
	switch level {
	case "debug":
		l.logger.SetLevel(logrus.DebugLevel)
	case "info":
		l.logger.SetLevel(logrus.InfoLevel)
	case "warn":
		l.logger.SetLevel(logrus.WarnLevel)
	case "error":
		l.logger.SetLevel(logrus.ErrorLevel)
	default:
		l.logger.SetLevel(logrus.InfoLevel)
	}
}

// SetOutput sets the output destination
func (l *Logger) SetOutput(w io.Writer) {
	l.logger.SetOutput(w)
}

// Debug logs a debug message with fields
func (l *Logger) Debug(msg string, fields ...interface{}) {
	l.logger.WithFields(parseFields(fields...)).Debug(msg)
}

// Info logs an info message with fields
func (l *Logger) Info(msg string, fields ...interface{}) {
	l.logger.WithFields(parseFields(fields...)).Info(msg)
}

// Warn logs a warning message with fields
func (l *Logger) Warn(msg string, fields ...interface{}) {
	l.logger.WithFields(parseFields(fields...)).Warn(msg)
}

// Error logs an error message with fields
func (l *Logger) Error(msg string, fields ...interface{}) {
	l.logger.WithFields(parseFields(fields...)).Error(msg)
}

// WithField returns a logger with a single field
func (l *Logger) WithField(key string, value interface{}) *Logger {
	return &Logger{logger: l.logger.WithField(key, value).Logger}
}

// WithFields returns a logger with multiple fields
func (l *Logger) WithFields(fields map[string]interface{}) *Logger {
	return &Logger{logger: l.logger.WithFields(logrus.Fields(fields)).Logger}
}

// parseFields converts variadic arguments to logrus.Fields
func parseFields(fields ...interface{}) logrus.Fields {
	result := make(logrus.Fields)

	for i := 0; i < len(fields); i += 2 {
		if i+1 < len(fields) {
			key, ok := fields[i].(string)
			if ok {
				result[key] = fields[i+1]
			}
		}
	}

	return result
}

// LogEvent logs a structured event
func (l *Logger) LogEvent(eventType, member string, data map[string]interface{}) {
	fields := logrus.Fields{
		"event_type": eventType,
		"member":     member,
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Info("event")
}

// LogMetrics logs member metrics
func (l *Logger) LogMetrics(member string, metrics map[string]interface{}) {
	fields := logrus.Fields{
		"member": member,
	}

	for k, v := range metrics {
		fields[k] = v
	}

	l.logger.WithFields(fields).Debug("metrics")
}

// LogDecision logs a decision event
func (l *Logger) LogDecision(decisionType, from, to string, data map[string]interface{}) {
	fields := logrus.Fields{
		"decision_type": decisionType,
		"from":          from,
		"to":            to,
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Info("decision")
}

// LogDiscovery logs member discovery events
func (l *Logger) LogDiscovery(member, class, iface string, data map[string]interface{}) {
	fields := logrus.Fields{
		"member": member,
		"class":  class,
		"iface":  iface,
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Info("discovery")
}

// LogError logs an error with context
func (l *Logger) LogError(err error, context map[string]interface{}) {
	fields := logrus.Fields{
		"error": err.Error(),
	}

	for k, v := range context {
		fields[k] = v
	}

	l.logger.WithFields(fields).Error("error")
}

// LogConfig logs configuration changes
func (l *Logger) LogConfig(action string, data map[string]interface{}) {
	fields := logrus.Fields{
		"action": action,
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Info("config")
}

// LogThrottle logs throttling events
func (l *Logger) LogThrottle(what string, cooldownS, remainingS int) {
	l.logger.WithFields(logrus.Fields{
		"what":        what,
		"cooldown_s":  cooldownS,
		"remaining_s": remainingS,
	}).Warn("throttle")
}

// LogSwitch logs interface switching events
func (l *Logger) LogSwitch(from, to, reason string, delta float64, data map[string]interface{}) {
	fields := logrus.Fields{
		"from":   from,
		"to":     to,
		"reason": reason,
		"delta":  delta,
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Info("switch")
}

// LogSample logs metric samples (debug level)
func (l *Logger) LogSample(member string, metrics map[string]interface{}) {
	fields := logrus.Fields{
		"member": member,
	}

	for k, v := range metrics {
		fields[k] = v
	}

	l.logger.WithFields(fields).Debug("sample")
}

// LogProvider logs provider selection and errors
func (l *Logger) LogProvider(member, provider string, data map[string]interface{}) {
	fields := logrus.Fields{
		"member":   member,
		"provider": provider,
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Info("provider")
}

// LogMWAN3 logs mwan3-related events
func (l *Logger) LogMWAN3(action string, data map[string]interface{}) {
	fields := logrus.Fields{
		"action": action,
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Info("mwan3")
}

// LogMemory logs memory usage and pressure events
func (l *Logger) LogMemory(usageMB, maxMB int, action string) {
	l.logger.WithFields(logrus.Fields{
		"usage_mb": usageMB,
		"max_mb":   maxMB,
		"action":   action,
	}).Warn("memory")
}

// LogPerformance logs performance metrics
func (l *Logger) LogPerformance(operation string, duration time.Duration, data map[string]interface{}) {
	fields := logrus.Fields{
		"operation": operation,
		"duration":  duration.String(),
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Debug("performance")
}

// LogStartup logs startup information
func (l *Logger) LogStartup(version string, data map[string]interface{}) {
	fields := logrus.Fields{
		"version": version,
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Info("startup")
}

// LogShutdown logs shutdown information
func (l *Logger) LogShutdown(reason string, data map[string]interface{}) {
	fields := logrus.Fields{
		"reason": reason,
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Info("shutdown")
}

// LogUptime logs uptime and health information
func (l *Logger) LogUptime(uptime time.Duration, data map[string]interface{}) {
	fields := logrus.Fields{
		"uptime": uptime.String(),
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Info("uptime")
}

// LogValidation logs validation events
func (l *Logger) LogValidation(component string, valid bool, data map[string]interface{}) {
	fields := logrus.Fields{
		"component": component,
		"valid":     valid,
	}

	for k, v := range data {
		fields[k] = v
	}

	level := logrus.InfoLevel
	if !valid {
		level = logrus.WarnLevel
	}

	l.logger.WithFields(fields).Log(level, "validation")
}

// LogReload logs configuration reload events
func (l *Logger) LogReload(source string, data map[string]interface{}) {
	fields := logrus.Fields{
		"source": source,
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Info("reload")
}

// LogHeartbeat logs periodic heartbeat information
func (l *Logger) LogHeartbeat(interval time.Duration, data map[string]interface{}) {
	fields := logrus.Fields{
		"interval": interval.String(),
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Debug("heartbeat")
}

// Trace logs a trace message with fields (most verbose level)
func (l *Logger) Trace(msg string, fields ...interface{}) {
	l.logger.WithFields(parseFields(fields...)).Trace(msg)
}

// LogVerbose logs verbose information for troubleshooting
func (l *Logger) LogVerbose(operation string, data map[string]interface{}) {
	fields := logrus.Fields{
		"operation": operation,
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Trace("verbose")
}

// LogDebugVerbose logs debug information with full context
func (l *Logger) LogDebugVerbose(operation string, data map[string]interface{}) {
	fields := logrus.Fields{
		"operation": operation,
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Debug("debug_verbose")
}

// LogStateChange logs state changes with full context
func (l *Logger) LogStateChange(component, fromState, toState string, reason string, data map[string]interface{}) {
	fields := logrus.Fields{
		"component":  component,
		"from_state": fromState,
		"to_state":   toState,
		"reason":     reason,
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Info("state_change")
}

// LogDataFlow logs data flow between components
func (l *Logger) LogDataFlow(from, to string, dataType string, dataSize int, data map[string]interface{}) {
	fields := logrus.Fields{
		"from":      from,
		"to":        to,
		"data_type": dataType,
		"data_size": dataSize,
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Debug("data_flow")
}

// LogTiming logs timing information for performance analysis
func (l *Logger) LogTiming(operation string, duration time.Duration, data map[string]interface{}) {
	fields := logrus.Fields{
		"operation":   operation,
		"duration_ms": duration.Milliseconds(),
		"duration_ns": duration.Nanoseconds(),
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Debug("timing")
}

// LogResourceUsage logs resource usage information
func (l *Logger) LogResourceUsage(resourceType string, usage float64, limit float64, unit string, data map[string]interface{}) {
	fields := logrus.Fields{
		"resource_type": resourceType,
		"usage":         usage,
		"limit":         limit,
		"unit":          unit,
		"usage_pct":     (usage / limit) * 100,
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Debug("resource_usage")
}

// LogNetworkActivity logs network-related activities
func (l *Logger) LogNetworkActivity(activity string, interfaceName string, data map[string]interface{}) {
	fields := logrus.Fields{
		"activity":  activity,
		"interface": interfaceName,
	}

	for k, v := range data {
		fields[k] = v
	}

	l.logger.WithFields(fields).Debug("network_activity")
}

// LogSystemCall logs system calls and their results
func (l *Logger) LogSystemCall(command string, args []string, exitCode int, stdout string, stderr string, duration time.Duration) {
	fields := logrus.Fields{
		"command":     command,
		"args":        args,
		"exit_code":   exitCode,
		"stdout_len":  len(stdout),
		"stderr_len":  len(stderr),
		"duration_ms": duration.Milliseconds(),
	}

	// Only log stdout/stderr if they're not too long
	if len(stdout) > 0 && len(stdout) < 1000 {
		fields["stdout"] = stdout
	}
	if len(stderr) > 0 && len(stderr) < 1000 {
		fields["stderr"] = stderr
	}

	level := logrus.InfoLevel
	if exitCode != 0 {
		level = logrus.WarnLevel
	}

	l.logger.WithFields(fields).Log(level, "system_call")
}

// LogAPICall logs API calls and their responses
func (l *Logger) LogAPICall(method, url string, statusCode int, responseTime time.Duration, data map[string]interface{}) {
	fields := logrus.Fields{
		"method":           method,
		"url":              url,
		"status_code":      statusCode,
		"response_time_ms": responseTime.Milliseconds(),
	}

	for k, v := range data {
		fields[k] = v
	}

	level := logrus.InfoLevel
	if statusCode >= 400 {
		level = logrus.WarnLevel
	}
	if statusCode >= 500 {
		level = logrus.ErrorLevel
	}

	l.logger.WithFields(fields).Log(level, "api_call")
}

// LogConfiguration logs configuration changes and validation
func (l *Logger) LogConfiguration(action string, configPath string, valid bool, data map[string]interface{}) {
	fields := logrus.Fields{
		"action":      action,
		"config_path": configPath,
		"valid":       valid,
	}

	for k, v := range data {
		fields[k] = v
	}

	level := logrus.InfoLevel
	if !valid {
		level = logrus.WarnLevel
	}

	l.logger.WithFields(fields).Log(level, "configuration")
}

// LogHealthCheck logs health check results
func (l *Logger) LogHealthCheck(component string, status string, details map[string]interface{}) {
	fields := logrus.Fields{
		"component": component,
		"status":    status,
	}

	for k, v := range details {
		fields[k] = v
	}

	level := logrus.InfoLevel
	switch status {
	case "healthy":
		level = logrus.DebugLevel
	case "degraded":
		level = logrus.WarnLevel
	case "critical":
		level = logrus.ErrorLevel
	}

	l.logger.WithFields(fields).Log(level, "health_check")
}
