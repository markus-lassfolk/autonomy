ChatGPT
--------------------
Let me summarize what we already have (code + API), highlight gaps, and give you a concrete, ordered plan to reach the ML monitoring you envision.

### What we have in code today (high level)
- Starlink collection and analysis
  - `starlink_comprehensive.c`: pulls GPS (lat/lon/alt, accuracy, velocity, HDOP, source), status (obstruction, SNR, pop latency/loss, device state), and runs a basic events/outages analysis window; background threads run collection + analysis.
  - `starlink_collector.c`: periodic status collection and health score; records obstruction observations to the obstruction engine.
  - `starlink_obstruction.c`: maintains time-series for obstruction and SNR; learns “environmental patterns” using k-NN-like matching with features: time-of-day/weekend, weather, and a crude location cluster; caches match history and does basic trend/anomaly checks.
  - Snow integration: `starlink_snow_detection*.c` uses real weather + obstruction/SNR trends to prewarm/heat dish with UBUS control hooks.
- Location, weather, terrain
  - `gps_weather.c`: current weather, forecast, air quality from OpenWeather; transparent caching; easy callable API.
  - `gps_terrain.c`: elevation, terrain features/roughness/slope from elevation APIs + SRTM; can characterize environment around dish.
- Telemetry/ML hooks
  - `telemetry_comprehensive_ubus.c`: exposes structured telemetry and an “execute ML algorithm” endpoint that can load models, export historical samples, and run an external Python ML executor on time windows.

### What we can collect from Starlink gRPC (docs/api-reference)
- From `get_status`/`get_history`/`get_location`/`get_diagnostics`:
  - Obstruction: `fractionObstructed`, `currentlyObstructed`, wedge arrays.
  - RF: `snr`, `isSnrAboveNoiseFloor`, `isSnrPersistentlyLow`.
  - Network: `popPingLatencyMs`, `popPingDropRate`, throughputs.
  - GPS: `gpsValid`, `gpsSats` (GNSS), accuracy/uncertainty, timestamp, source.
  - Device: thermal alerts, uptime, software-update flags.
- Not exposed: a stable Starlink satellite ID currently in use; “satellites-in-view” for Starlink isn’t present. We can estimate Starlink sats-in-view via TLE/SGP4.

### Gaps to close for your goals
- Outage events: `collect_from_history_api` currently tries HTTP endpoints that are not officially available; switch to gRPC `get_history` and persist structured outage/events.
- Satellite visibility: no per-Starlink satellite ID; add TLE ingestion and SGP4 pass prediction to estimate sats-in-view and sky geometry.
- Obstruction map: current “patterns” don’t maintain a true azimuth×elevation sky grid; we should build and persist a proper sky heatmap from wedge arrays + boresight.
- Root-cause labels: no unified, consistent labeler for obstruction vs weather vs thermal vs upstream/network; add rules now, ML later.
- Reliability: no per-satellite or per-plane/beam reliability rollups; add hierarchical reliability (shell/plane/time-of-day/ground path).

### Ordered implementation plan
1) Harden data ingestion and persistence
- Replace HTTP “history” with gRPC `get_history`; persist structured OutageEvent with: t_start/t_end/duration, cause flags (obstructed, thermal, SW update, backend), pre/post SNR/latency/loss, boresight, wedge arrays, GPS validity/accuracy.
- Persist Observation snapshots at a steady cadence (e.g., 5–15s) with obstruction, SNR, pop metrics, thermal flags, GPS.

2) Build a real obstruction sky map
- Create azimuth×elevation grid (e.g., 2°×2°). Map Starlink wedge indices to azimuth bins and use `boresightElevationDeg` with wedge geometry to distribute evidence.
- Update grid per observation: increase obstruction probability where wedge obstruction and low SNR coincide; decay over time.
- Expose via UBUS: get/set, export, and small PNG/JSON for UI.

3) Add TLE-based Starlink satellite visibility
- Use `CRED_SERVICE_SPACE_TRACK` to fetch Starlink TLEs; run SGP4 to compute satellites above a chosen elevation cutoff at the dish’s lat/lon.
- Log sats-in-view count and aggregate by orbital plane/time-of-day; if feasible, keep a small “likely pass direction” vector for correlation with sky bins.
- Start with a small C SGP4 lib or call a sandboxed Python helper; cache TLEs daily.

4) Root-cause labeling (rules MVP)
- Create a rules engine that tags each outage as:
  - obstruction (fractionObstructed high OR wedge/local sky bins high AND SNR low),
  - weather fade (rain/snow + SNR low, obstruction map low),
  - terminal/system (thermal throttle/shutdown or SW update flags),
  - upstream/network (many good-SNR observations around outage with high `popPingDropRate`/latency).
- Track “controllable vs non-controllable” = obstruction/weather vs terminal/system/upstream.

5) Reliability scoring (hierarchical)
- Maintain exposure vs dropout for:
  - sky cells (az×el bins),
  - predicted visibility contexts (e.g., azimuth sector/time-of-day),
  - orbital planes/shell if we want to attribute by predicted plane presence,
  - upstream/ground path proxy (high pop loss/lat spikes buckets).
- Empirical-Bayes shrinkage so sparse buckets don’t overfit; expose reliability and confidence.

6) ML upgrade path
- Export an ML dataset (extend `export_ml_dataset`): features per outage and pre/post window: obstruction stats, sky-bin obstruction densities, SNR trends, pop metrics, weather (rain rate/temp/humidity), movement, seasonality, sats-in-view predicted.
- Train a calibrated classifier (e.g., gradient-boosted trees) for outage_cause and controllable flag. Measure PR-AUC/F1 and calibration (Brier).
- Integrate with `telemetry_comprehensive_ubus_execute_ml_algorithm` by shipping a versioned model artifact; return predicted cause, controllability, and confidence.

7) Productization and UI
- Add UBUS methods: get obstruction heatmap, reliability breakdowns, and predicted risk next 30–60 minutes (combine sky map + TLE + forecasted weather).
- Alerts: if entering high-risk sky bins or persistent upstream issues, annotate decisions and suggest actions.

8) Continuous improvement
- Active learning: route low-confidence events for manual tag; feed back new labels.
- Seasonality: decay factors in sky map; different models per season if helpful.
- Performance/privacy: retention windows and downsampling in `telemetry_store`.

### Feasibility notes
- Per-satellite reliability: dish doesn’t expose satellite IDs. Use TLE+SGP4 for “sats-in-view count” and contextual reliability (az/elev/time/plane). If a future API exposes sat IDs, plug it into the same reliability layer.
- “Controllable” rubric aligns well with available signals: obstruction/wedge, SNR trend, weather, thermal flags, and upstream packet loss/latency.

### Where this lives
- Data/logic in `src/c/autonomy-daemon/starlink/*`, `gps/*`, `telemetry/*`; new modules for TLE/SGP4 and sky grid; docs in `docs/` with a brief design spec and API additions (I’ll place it under `docs/monitoring/starlink-ml-plan.md` to match your docs preference [[memory:8355723]]).

- Completed: inventoried Starlink/location/weather code; reviewed API docs; summarized capabilities/gaps; drafted an ordered ML plan. 
