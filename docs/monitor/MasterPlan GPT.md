# Master Plan: Starlink Obstruction, Satellite Visibility, and ML-Based Outage Intelligence (GPT)

## Executive summary

- Unify outage intelligence across Starlink telemetry, local sky obstruction mapping,
  predicted satellite visibility, weather, and location.
- Deliver a stepwise path: hardened data ingestion → true sky obstruction map →
  satellite visibility via TLE/SGP4 → rules-based root-cause labeling → hierarchical
  reliability → calibrated ML classification → predictive risk/avoidance → UI and
  continuous learning.
- Aligns and consolidates the three plans (Auto, ChatGPT, Gemini), grounded in the
  current C-based autonomy daemon and Starlink gRPC capabilities.

### Goals and non-goals

- Goals
  - Build an accurate, continuously learning obstruction map (azimuth×elevation) tied
    to real dish telemetry.
  - Correlate outages with predicted satellites-in-view and weather/location context.
  - Distinguish controllable vs non-controllable causes; prioritize actionable
    remediations.
  - Score reliability across sky cells, time-of-day, orbital planes/coverage contexts,
    and upstream link health.
  - Predict near-term risk (5–60 minutes) and surface proactive actions.
- Non-goals (initially)
  - Per-Starlink-satellite identification via dish API (IDs are not exposed). We will
    approximate via TLE visibility and context.
  - Heavy external dependencies or complex DB migrations; prefer lightweight local
    storage and existing telemetry pathways.

### Current foundation (what we have)

- Starlink collection and analysis
  - `src/c/autonomy-daemon/starlink/starlink_comprehensive.c` collects GPS (lat/lon/alt,
    accuracy, velocity, HDOP, source), basic status (obstruction, SNR, PoP latency/loss,
    device state), and runs a rolling events/outages analysis.
  - `starlink_collector.c` periodically collects status, computes a health score, and forwards obstruction observations.
  - `starlink_obstruction.c` maintains obstruction and SNR trend histories, matches simple environmental patterns, and detects anomalies.
  - Snow integration `starlink_snow_detection*.c` leverages weather + obstruction/SNR trends for heating automation (useful seasonal signal).
- Location, weather, terrain
  - `gps_weather.c` integrates OpenWeather current/forecast/AQI with caching.
  - `gps_terrain.c` provides elevation, slope/roughness and environment context using elevation APIs and SRTM.
- Telemetry/ML hooks
  - `telemetry_comprehensive_ubus.c` exposes historical samples and an ML execution hook (`execute_ml_algorithm`) for external models.

### Starlink API capabilities we will use

- gRPC: `get_status`, `get_history`, `get_location`, `get_diagnostics`.
- Key fields: obstruction fraction and wedge arrays, SNR and its flags, PoP latency/drop rate, GPS validity/sat count/uncertainty, thermal/update flags, boresight azimuth/elevation.
- Limitation: no explicit serving satellite ID; no native Starlink “sats-in-view” listing.

### Architecture overview (target)

1. Ingestion & persistence
   - Replace any HTTP history attempts with gRPC `get_history`. Persist OutageEvent snapshots and regular Observations (e.g., every 5–15s).
2. Sky obstruction map service
   - Convert wedge obstruction + boresight to an azimuth×elevation grid with exponential decay; export via UBUS for UI/analytics.
3. Satellite visibility service
   - Ingest Starlink TLEs (CelesTrak or Space-Track) and run SGP4 to compute satellites above an elevation threshold for the dish location.
   - Expose “sats-in-view” count and az/el distributions over time; aggregate by orbital plane/shell/time-of-day.
4. Root-cause labeling & reliability
   - Rules-based classifier for outage cause and controllability; hierarchical reliability (sky cell → az sector/time → plane/shell → upstream path).
5. ML & prediction
   - Export features, train calibrated models for cause/controllability; combine sky map + TLE + weather to predict near-term risk and advise actions.

### Data model (lightweight additions)

- Observation (periodic)
  - timestamp, `fraction_obstructed`, wedge arrays, `snr_db`, PoP latency/loss, boresight az/el, GPS validity/accuracy, temperature, precipitation proxy/clouds.
- OutageEvent (edge-triggered)
  - t_start/t_end/duration, reason flags (obstructed, thermal, update, backend), pre/post SNR and PoP metrics, GPS confidence, sky bins snapshot.
- SkyCell (az_bin, el_bin)
  - obstruction_prob, evidence_count, last_updated, seasonal_decay_state.
- VisibilityContext (optional)
  - timestamp, sats_in_view_count, predicted az/el density moments, orbital_plane_presence.
- ReliabilityBucket
  - key_type ∈ {sky_cell, az_sector_time, orbital_plane, upstream_path}, key_id, exposure_ms, dropout_ms, score, confidence.

### Phased roadmap and acceptance criteria

#### Phase 0 — Readiness (1 week)

- Tasks
  - Confirm gRPC `get_history` integration feasibility from daemon environment.
  - Finalize az/el grid spec (e.g., 2°×2°, elevation cap 90°, azimuth wrap).
  - Choose SGP4 implementation path (embedded C library or safe Python helper).
- Acceptance
  - Design notes checked in under `docs/monitor/` for history ingestion, sky-grid math, and visibility API contract.

#### Phase 1 — Harden ingestion & persistence (1–2 weeks)

- Implement
  - Switch history collector to gRPC; persist OutageEvent and periodic Observations (append-only, capped retention).
  - Extend `telemetry_comprehensive_ubus` to export these records for dashboards/ML export.
- Acceptance
  - Outage and observation streams visible via UBUS and basic CLI tooling; 7–30 days retention configurable.

#### Phase 2 — Real obstruction sky map (1–2 weeks)

- Implement
  - Map wedge arrays to azimuth sectors; estimate elevation contribution using `boresightElevationDeg` and a per-wedge vertical profile.
  - Update sky grid per observation; apply exponential decay and seasonal decay.
  - Add UBUS methods: get/set/export sky map, and small PNG/JSON for UI.
- Acceptance
  - Repeatable obstruction hotspots persist in expected az/el bins; visual inspection matches known obstacles.

#### Phase 3 — Satellite visibility via TLE/SGP4 (1–2 weeks)

- Implement
  - TLE fetcher (CelesTrak or Space-Track); daily update and validation.
  - SGP4 computation to produce “sats-in-view” and az/el lists given timestamp and dish location; expose via internal API.
  - Log `sats_in_view_count` + coarse azimuth density during observations.
- Acceptance
  - Visibility results sanity-checked against external trackers; performance acceptable on target hardware.

#### Phase 4 — Rules-based root-cause & controllability (1 week)

- Implement rules
  - Obstruction: high fraction/wedge bins + low SNR.
  - Weather fade: precipitation/cloud signals + low SNR but low obstruction.
  - Terminal/system: thermal throttle/shutdown or update flags.
  - Upstream/network: good SNR and obstruction low, but high PoP loss/latency.
  - Controllable = obstruction/weather; Non-controllable = terminal/system/upstream.
- Acceptance
  - Confusion matrix on a curated set shows expected separations; labels applied to historical OutageEvents.

#### Phase 5 — Hierarchical reliability (1–2 weeks)

- Implement
  - Track exposure vs dropout across reliability buckets (sky cell, az-sector×time, plane/shell proxy, upstream path buckets).
  - Apply empirical-Bayes shrinkage; expose score + confidence.
- Acceptance
  - Reliability heatmaps correlate with future outage probability in holdout periods.

#### Phase 6 — ML cause/controllability (2–3 weeks data collection, 1 week model)

- Implement
  - Extend ML export with per-event features: sky-bin densities, SNR trends, PoP metrics, weather, movement, seasonality, sats-in-view.
  - Train calibrated classifier (e.g., gradient-boosted trees). Evaluate PR-AUC/F1 per class and Brier score; implement confidence thresholds.
  - Integrate model artifact with `execute_ml_algorithm` for online inference.
- Acceptance
  - Target ≥0.8 PR-AUC for obstruction vs non-obstruction; well-calibrated probabilities.

#### Phase 7 — Predictive risk & avoidance (1–2 weeks)

- Implement
  - Forecast risk for next 5–60 minutes combining sky map + predicted passes + forecast weather and time-of-day.
  - Surface actions: schedule heavy tasks in low-risk windows; suggest mount adjustments; raise alerts when entering high-risk bins.
- Acceptance
  - Prospective alerts captured; measurable reduction in avoidable outages over a trial period.

#### Phase 8 — Productization & UI (parallel, 1–2 weeks)

- Implement
  - New UBUS endpoints: sky map export, reliability breakdowns, predictive risk API.
  - Minimal dashboard panels (Grafana or built-in web UI) for sky map, reliability, and risk timeline.
- Acceptance
  - Operators can inspect cause labels, reliability hotspots, and upcoming risk.

#### Phase 9 — Continuous learning & ops (ongoing)

- Active learning: flag low-confidence events for manual labels; feed back to training.
- Seasonality handling: decay/season switches; per-season models if useful.
- Retention and privacy: downsample, anonymize coordinates when needed.

### Implementation specifics and interfaces

- History ingestion
  - Replace any HTTP polling in `starlink_comprehensive.c` with gRPC for `get_history`; model OutageEvent in daemon structs and persist.
- Sky grid
  - Maintain a fixed az×el grid (e.g., 180×45 for 2° bins). Update with wedge evidence; store `obstruction_prob` and counts.
- Visibility service
  - A small module (e.g., `starlink_visibility.[c|h]`) or a sandboxed helper exposing a simple interface: `get_visible_sats(lat, lon, ts) → [{az, el}]` and `count`.
- Reliability
  - Bucketing and shrinkage implemented in C for transparency; expose via UBUS.
- ML hooks
  - Use `telemetry_comprehensive_ubus` to export datasets and to run an external Python model; version model artifacts and log predictions.

### Evaluation metrics

- Classifier: PR-AUC/F1 by class; calibration (Brier). Confidence thresholds to gate actions.
- Obstruction map: correlation between high-probability bins and future obstruction events; post-adjustment improvement.
- Reliability: stability and predictive power on holdout periods.
- Business: reduction in controllable outages; alert precision/recall; time-to-detect degradations.

### Risks and mitigations

- No per-satellite ID from dish
  - Mitigation: use visibility context (sats-in-view, az/el density, orbital planes) and sky-cell reliability; if IDs become available later, plug into the same framework.
- Weather confounds SNR
  - Mitigation: include weather features and use both obstruction map and SNR trends; calibrate models.
- Seasonal foliage and mobility
  - Mitigation: apply time-decay and season-aware factors; use movement detection to gate learning when in motion.
- Performance on embedded targets
  - Mitigation: keep SGP4 helper lightweight; cache TLEs; downsample observations as needed.

### Deliverables checklist

- gRPC history ingestion with persisted OutageEvents and Observations.
- Sky obstruction grid UBUS API and export.
- TLE/SGP4 visibility service and logs.
- Rules-based root-cause and controllability labels.
- Reliability scoring with hierarchical buckets and confidence.
- ML classifier integrated via `execute_ml_algorithm` with metrics.
- Predictive risk API and minimal dashboard.

### References mapped to the three plans

- From Auto: richer data schema, satellite reliability scoring, predictive outage detection, multi-phase cadence.
- From ChatGPT: rules-first labeling, sky grid from wedge arrays, hierarchical reliability and calibrated ML.
- From Gemini: explicit TLE/SGP4 visibility service and visualization-first labeling loop.

This master plan sequences the lowest-risk, highest-leverage steps first (ingestion, sky map, visibility), then layers reliability, labeling, and ML, culminating in predictive risk and a practical operator-facing UI—all within the existing autonomy daemon architecture.
