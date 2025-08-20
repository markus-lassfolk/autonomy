package metrics

import (
	"context"
	"fmt"
	"net/http"
	"time"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"github.com/markus-lassfolk/autonomy/pkg"
	"github.com/markus-lassfolk/autonomy/pkg/controller"
	"github.com/markus-lassfolk/autonomy/pkg/decision"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
	"github.com/markus-lassfolk/autonomy/pkg/telem"
)

// Server provides Prometheus metrics for autonomyd
type Server struct {
	controller *controller.Controller
	decision   *decision.Engine
	store      *telem.Store
	logger     *logx.Logger
	server     *http.Server

	// Prometheus metrics
	memberScore       *prometheus.GaugeVec
	memberLatency     *prometheus.GaugeVec
	memberLoss        *prometheus.GaugeVec
	memberSignal      *prometheus.GaugeVec
	memberObstruction *prometheus.GaugeVec
	memberOutages     *prometheus.CounterVec
	memberSwitches    *prometheus.CounterVec
	memberUptime      *prometheus.GaugeVec
	memberStatus      *prometheus.GaugeVec

	decisionCycles   *prometheus.CounterVec
	decisionErrors   *prometheus.CounterVec
	collectionErrors *prometheus.CounterVec

	telemetrySamples     *prometheus.GaugeVec
	telemetryEvents      *prometheus.GaugeVec
	telemetryMemoryUsage *prometheus.GaugeVec

	daemonUptime  *prometheus.GaugeVec
	daemonVersion *prometheus.GaugeVec
}

// NewServer creates a new metrics server
func NewServer(ctrl *controller.Controller, eng *decision.Engine, store *telem.Store, logger *logx.Logger) *Server {
	s := &Server{
		controller: ctrl,
		decision:   eng,
		store:      store,
		logger:     logger,
	}

	s.registerMetrics()
	return s
}

// registerMetrics registers all Prometheus metrics
func (s *Server) registerMetrics() {
	// Member metrics
	s.memberScore = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "autonomy_member_score",
			Help: "Current health score for each member",
		},
		[]string{"member", "class", "interface"},
	)

	s.memberLatency = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "autonomy_member_latency_ms",
			Help: "Current latency for each member in milliseconds",
		},
		[]string{"member", "class", "interface"},
	)

	s.memberLoss = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "autonomy_member_loss_percent",
			Help: "Current packet loss percentage for each member",
		},
		[]string{"member", "class", "interface"},
	)

	s.memberSignal = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "autonomy_member_signal_dbm",
			Help: "Current signal strength for each member in dBm",
		},
		[]string{"member", "class", "interface"},
	)

	s.memberObstruction = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "autonomy_member_obstruction_percent",
			Help: "Current obstruction percentage for Starlink members",
		},
		[]string{"member", "class", "interface"},
	)

	s.memberOutages = prometheus.NewCounterVec(
		prometheus.CounterOpts{
			Name: "autonomy_member_outages_total",
			Help: "Total number of outages for each member",
		},
		[]string{"member", "class", "interface"},
	)

	s.memberSwitches = prometheus.NewCounterVec(
		prometheus.CounterOpts{
			Name: "autonomy_member_switches_total",
			Help: "Total number of switches to each member",
		},
		[]string{"member", "class", "interface"},
	)

	s.memberUptime = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "autonomy_member_uptime_seconds",
			Help: "Current uptime for each member in seconds",
		},
		[]string{"member", "class", "interface"},
	)

	s.memberStatus = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "autonomy_member_status",
			Help: "Current status of each member (1=active, 0=inactive)",
		},
		[]string{"member", "class", "interface", "state"},
	)

	// Decision engine metrics
	s.decisionCycles = prometheus.NewCounterVec(
		prometheus.CounterOpts{
			Name: "autonomy_decision_cycles_total",
			Help: "Total number of decision engine cycles",
		},
		[]string{"result"},
	)

	s.decisionErrors = prometheus.NewCounterVec(
		prometheus.CounterOpts{
			Name: "autonomy_decision_errors_total",
			Help: "Total number of decision engine errors",
		},
		[]string{"type"},
	)

	s.collectionErrors = prometheus.NewCounterVec(
		prometheus.CounterOpts{
			Name: "autonomy_collection_errors_total",
			Help: "Total number of metric collection errors",
		},
		[]string{"member", "class", "type"},
	)

	// Telemetry metrics
	s.telemetrySamples = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "autonomy_telemetry_samples",
			Help: "Number of samples in telemetry store",
		},
		[]string{"member"},
	)

	s.telemetryEvents = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "autonomy_telemetry_events",
			Help: "Number of events in telemetry store",
		},
		[]string{"type"},
	)

	s.telemetryMemoryUsage = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "autonomy_telemetry_memory_bytes",
			Help: "Memory usage of telemetry store in bytes",
		},
		[]string{"type"},
	)

	// Daemon metrics
	s.daemonUptime = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "autonomy_daemon_uptime_seconds",
			Help: "Daemon uptime in seconds",
		},
		[]string{},
	)

	s.daemonVersion = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "autonomy_daemon_version_info",
			Help: "Daemon version information",
		},
		[]string{"version", "go_version"},
	)

	// Register all metrics
	prometheus.MustRegister(
		s.memberScore,
		s.memberLatency,
		s.memberLoss,
		s.memberSignal,
		s.memberObstruction,
		s.memberOutages,
		s.memberSwitches,
		s.memberUptime,
		s.memberStatus,
		s.decisionCycles,
		s.decisionErrors,
		s.collectionErrors,
		s.telemetrySamples,
		s.telemetryEvents,
		s.telemetryMemoryUsage,
		s.daemonUptime,
		s.daemonVersion,
	)
}

// Start starts the metrics server
func (s *Server) Start(port int) error {
	s.logger.Info("Starting metrics server", map[string]interface{}{
		"port": port,
	})

	mux := http.NewServeMux()
	mux.Handle("/metrics", promhttp.Handler())
	mux.HandleFunc("/health", s.healthHandler)

	s.server = &http.Server{
		Addr:    fmt.Sprintf(":%d", port),
		Handler: mux,
	}

	go func() {
		if err := s.server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			s.logger.Error("Metrics server error", map[string]interface{}{
				"error": err.Error(),
			})
		}
	}()

	return nil
}

// Stop stops the metrics server
func (s *Server) Stop() error {
	s.logger.Info("Stopping metrics server")

	if s.server != nil {
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		return s.server.Shutdown(ctx)
	}
	return nil
}

// healthHandler provides a simple health check endpoint
func (s *Server) healthHandler(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	if _, err := w.Write([]byte(`{"status":"healthy","timestamp":"` + time.Now().Format(time.RFC3339) + `"}`)); err != nil {
		s.logger.Error("Failed to write health response", "error", err)
	}
}

// UpdateMetrics updates all Prometheus metrics with current data
func (s *Server) UpdateMetrics() {
	s.updateMemberMetrics()
	s.updateTelemetryMetrics()
	s.updateDaemonMetrics()
}

// updateMemberMetrics updates member-related metrics
func (s *Server) updateMemberMetrics() {
	members := s.controller.GetMembers()
	activeMember, err := s.controller.GetActiveMember()
	if err != nil {
		activeMember = nil
	}

	for _, member := range members {
		labels := prometheus.Labels{
			"member":    member.Name,
			"class":     string(member.Class),
			"interface": member.Iface,
		}

		// Get latest metrics for this member
		samples, err := s.store.GetSamples(member.Name, time.Now().Add(-time.Minute))
		if err != nil {
			continue
		}
		if len(samples) > 0 {
			metrics := samples[0].Metrics
			score := samples[0].Score

			// Update score metric
			s.memberScore.With(labels).Set(score.Final)

			// Extract metrics from struct - handle pointers
			latency := 0.0
			if metrics.LatencyMS != nil {
				latency = *metrics.LatencyMS
			}
			loss := 0.0
			if metrics.LossPercent != nil {
				loss = *metrics.LossPercent
			}
			signal := 0.0
			if metrics.SignalStrength != nil {
				signal = float64(*metrics.SignalStrength)
			}
			obstruction := 0.0
			if metrics.ObstructionPct != nil {
				obstruction = *metrics.ObstructionPct
			}
			outages := 0.0
			if metrics.Outages != nil {
				outages = float64(*metrics.Outages)
			}

			// Update latency metric
			s.memberLatency.With(labels).Set(latency)

			// Update loss metric
			s.memberLoss.With(labels).Set(loss)

			// Update signal metric (if available)
			if signal != 0 {
				s.memberSignal.With(labels).Set(signal)
			}

			// Update obstruction metric (for Starlink)
			if member.Class == pkg.MemberClassStarlink && obstruction > 0 {
				s.memberObstruction.With(labels).Set(obstruction)
			}

			// Update outages metric
			s.memberOutages.With(labels).Add(outages)
		}

		// Determine actual member state
		memberState := "inactive"
		if member.Eligible {
			memberState = "eligible"
		}
		if activeMember != nil && activeMember.Name == member.Name {
			memberState = "active"
		}

		// Update status metric
		statusLabels := prometheus.Labels{
			"member":    member.Name,
			"class":     string(member.Class),
			"interface": member.Iface,
			"state":     memberState,
		}

		status := 0.0
		if activeMember != nil && activeMember.Name == member.Name {
			status = 1.0
		}
		s.memberStatus.With(statusLabels).Set(status)

		// Update uptime metric (simplified)
		if member.CreatedAt != (time.Time{}) {
			uptime := time.Since(member.CreatedAt).Seconds()
			s.memberUptime.With(labels).Set(uptime)
		}
	}
}

// updateTelemetryMetrics updates telemetry-related metrics
func (s *Server) updateTelemetryMetrics() {
	members := s.controller.GetMembers()

	// Update sample counts
	for _, member := range members {
		samples, err := s.store.GetSamples(member.Name, time.Now().Add(-time.Hour))
		if err == nil {
			s.telemetrySamples.With(prometheus.Labels{"member": member.Name}).Set(float64(len(samples)))
		}
	}

	// Update event counts
	events, err := s.store.GetEvents(time.Now().Add(-time.Hour), 1000)
	if err != nil {
		return
	}
	eventCounts := make(map[string]int)
	for _, event := range events {
		eventCounts[event.Type]++
	}

	for eventType, count := range eventCounts {
		s.telemetryEvents.With(prometheus.Labels{"type": eventType}).Set(float64(count))
	}

	// Update memory usage with actual telemetry store data
	totalMemoryUsage := s.store.GetMemoryUsage()

	// Estimate samples vs events memory usage (rough approximation)
	// Events are typically much smaller than samples
	samplesMemory := float64(totalMemoryUsage) * 0.8 // 80% for samples
	eventsMemory := float64(totalMemoryUsage) * 0.2  // 20% for events

	s.telemetryMemoryUsage.With(prometheus.Labels{"type": "samples"}).Set(samplesMemory)
	s.telemetryMemoryUsage.With(prometheus.Labels{"type": "events"}).Set(eventsMemory)
	s.telemetryMemoryUsage.With(prometheus.Labels{"type": "total"}).Set(float64(totalMemoryUsage))
}

// updateDaemonMetrics updates daemon-related metrics
func (s *Server) updateDaemonMetrics() {
	// Update uptime (simplified - would need actual start time tracking)
	s.daemonUptime.With(prometheus.Labels{}).Set(time.Since(time.Now().Add(-time.Hour)).Seconds())

	// Update version info
	s.daemonVersion.With(prometheus.Labels{
		"version":    "1.0.0",
		"go_version": "1.22",
	}).Set(1)
}

// RecordDecisionCycle records a decision engine cycle
func (s *Server) RecordDecisionCycle(result string) {
	s.decisionCycles.With(prometheus.Labels{"result": result}).Inc()
}

// RecordDecisionError records a decision engine error
func (s *Server) RecordDecisionError(errorType string) {
	s.decisionErrors.With(prometheus.Labels{"type": errorType}).Inc()
}

// RecordCollectionError records a metric collection error
func (s *Server) RecordCollectionError(member, class, errorType string) {
	s.collectionErrors.With(prometheus.Labels{
		"member": member,
		"class":  class,
		"type":   errorType,
	}).Inc()
}

// RecordMemberSwitch records a member switch
func (s *Server) RecordMemberSwitch(member pkg.Member) {
	s.memberSwitches.With(prometheus.Labels{
		"member":    member.Name,
		"class":     string(member.Class),
		"interface": member.Iface,
	}).Inc()
}
