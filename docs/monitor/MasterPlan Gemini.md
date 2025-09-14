# Starlink ML Monitoring: The Master Plan

## 1. Vision & Goals

This document outlines the master plan for creating an intelligent, ML-powered monitoring
system for our Starlink integration. The system will go beyond simple outage detection to
provide deep contextual analysis, predictive insights, and actionable recommendations.

**Core Goals:**

1. **Classify Outages:** Automatically determine the root cause of any outage,
   distinguishing between controllable (e.g., local obstructions) and uncontrollable
   (e.g., satellite network issues) events.
2. **Dynamic Obstruction Mapping:** Create a self-learning, high-resolution sky map that
   adapts to environmental changes over time.
3. **Satellite Reliability Tracking:** Score the performance and reliability of the Starlink
   constellation on a contextual basis (per sky region, time of day, etc.).
4. **Predictive Analytics:** Forecast periods of high risk for outages or degraded performance
   based on satellite paths, weather, and historical data.

---

## 2. Synthesis of Approaches

This master plan synthesizes three distinct analyses:

* **Gemini's Strategic Roadmap:** Provided a high-level, phased approach emphasizing the
  need for external satellite tracking data as a foundational component.
* **Auto's Data-Centric Plan:** Offered detailed data schemas and a strong focus on
  integrating multiple data sources, including weather, for rich feature engineering.
* **ChatGPT's Code-Pragmatic Plan:** Identified specific gaps in the current C codebase and
  proposed concrete, incremental steps for building a rules-based MVP before advancing to
  complex ML.

By merging these views, we get a plan that is strategically sound, technically detailed, and
pragmatically implementable.

---

## 3. The Phased Implementation Plan

The project is broken down into five logical phases, designed to deliver value at each stage
and build upon the previous phase's accomplishments.

### Phase 1: Harden Data Foundation & Integration (Weeks 1-3)

**Goal:** Ensure we are capturing all necessary data reliably and storing it in a structured, accessible format. This phase closes critical gaps in our current data collection and leverages our existing, powerful satellite tracking code.

* **1.1. Fix Starlink History Ingestion:**
  * **Action:** Modify the data collector in `starlink_collector.c` to use the gRPC `get_history` method instead of the non-functional HTTP endpoints.
  * **Details:** Persist structured outage events, capturing `t_start`, `duration`, `cause_flags` (from the dish), pre/post-outage performance metrics (SNR, latency, drop rate), and dish boresight data at the time of the event.

* **1.2. Integrate Existing Satellite Visibility Engine:**
  * **Action:** Ensure the existing C-based satellite tracking engine (`prediction_engine.c` and `dynamic_satellite_tracker.c`) is properly integrated into our main data collection loop. The plan to create a new Python service was incorrect; this capability already exists.
  * **Details:**
    * Confirm that a TLE data source (e.g., from Celestrak) is being regularly fetched and loaded into the `prediction_engine`.
    * At each observation/event, call the appropriate functions to get the count of all visible satellites. This data point is critical context for every observation.
    * Log the identified serving satellite (from `dynamic_satellite_tracker.c`) when an identification is successful.

* **1.3. Establish Centralized Time-Series Database:**
  * **Action:** Set up a TimescaleDB (PostgreSQL) or InfluxDB instance to store all monitoring data.
  * **Schema:** Implement a schema based on the detailed `structs` proposed in the "Auto" plan. This ensures all data points are captured for future ML feature engineering.

    ```sql
    -- Primary Observation Table
    CREATE TABLE starlink_observations (
        timestamp TIMESTAMPTZ PRIMARY KEY,
        -- Starlink Performance
        pop_ping_latency_ms REAL,
        pop_ping_drop_rate REAL,
        snr REAL,
        fraction_obstructed REAL,
        currently_obstructed BOOLEAN,
        -- Dish State
        boresight_azimuth REAL,
        boresight_elevation REAL,
        thermal_throttle BOOLEAN,
        -- GPS State
        latitude REAL,
        longitude REAL,
        altitude REAL,
        gps_valid BOOLEAN,
        gps_sats INTEGER,
        -- External Context
        visible_satellites_count INTEGER, -- From TLE Service
        weather_condition TEXT,
        temperature REAL,
        cloud_cover REAL
    );
    ```

### Phase 2: High-Resolution Sky Mapping (Weeks 4-6)

**Goal:** Move from basic obstruction statistics to a persistent, high-fidelity map of the dish's field of view.

* **2.1. Create the Sky Map Grid:**
  * **Action:** Implement a new module (`starlink_sky_map.c`) that maintains an in-memory grid representing the sky (e.g., 2°x2° bins of azimuth and elevation).

* **2.2. Ingest Obstruction Data:**
  * **Action:** At each observation, use the `boresight` and `wedgeFractionObstructed` data from the Starlink API to update the grid. Distribute the obstruction "evidence" from the wedges into the corresponding sky map bins.
  * **Logic:** Increase the obstruction probability for bins where low SNR and high wedge obstruction coincide. Implement a decay factor so the map can change over time.

* **2.3. Expose the Map:**
  * **Action:** Add UBUS methods to get the current sky map grid, allowing for UI visualization and external analysis.

### Phase 3: Root Cause Analysis - Rules MVP & Data Correlation (Weeks 7-10)

**Goal:** Begin classifying outages using a deterministic rules engine and enrich all data with satellite context.

* **3.1. Build the Correlation Service:**
  * **Action:** Create a service that listens for new outage events. When an event occurs, it calls the **Satellite Visibility Service (1.2)** to get the list of satellites in view at that moment and enriches the event data in the database.

* **3.2. Implement a Rules-Based Classifier:**
  * **Action:** Create a "first-pass" labeling engine (`starlink_event_analyzer.c`) that tags each outage with a likely cause based on clear, deterministic rules.
  * **Rules MVP:**
    * **`OBSTRUCTION`**: If `fractionObstructed` is high OR the dish was pointing at a high-probability bin in our new sky map.
    * **`WEATHER_FADE`**: If weather service reports rain/snow AND SNR is low BUT the sky map shows low obstruction probability.
    * **`NO_SATELLITE_COVERAGE`**: If our Satellite Visibility Service reports zero visible satellites in the unobstructed view.
    * **`NETWORK_ISSUE`**: If `popPingDropRate`/`Latency` is high BUT SNR is good and obstruction is low.
    * **`TERMINAL_ISSUE`**: If a thermal or software update alert is active.

### Phase 4: Visualization & Initial ML Modeling (Weeks 11-15)

**Goal:** Visualize the correlated data to gain insights and train the first-generation ML model to automate the classification from Phase 3.

* **4.1. Develop Visualization Dashboard:**
  * **Action:** Use Grafana or a simple web app to build a dashboard.
  * **Key Panels:**
        1. **Sky Map:** Display the obstruction heatmap from **Phase 2** and overlay the locations of all historical outages.
        2. **Correlated Timeline:** Chart SNR, latency, and packet loss. Annotate the timeline with outage events, their rule-based labels, and the number of visible satellites.

* **4.2. Train the Classification Model:**
  * **Action:** Export a dataset from the database, using the rule-based tags from **Phase 3** as our ground-truth labels.
  * **Features:** Include all collected metrics: Starlink performance, weather data, satellite visibility counts, and obstruction values from the sky map bins.
  * **Model:** Train a Gradient Boosted Trees model (like `XGBoost` or `LightGBM`) to predict the outage cause.
  * **Integration:** Use the existing `telemetry_comprehensive_ubus_execute_ml_algorithm` hook to integrate the trained model.

### Phase 5: Advanced ML & Predictive Capabilities (Weeks 16+)

**Goal:** Move from classification to prediction and enable the advanced, self-learning capabilities.

* **5.1. Implement Self-Learning Sky Map:**
  * **Action:** Feed the ML model's `OBSTRUCTION` classifications back into the sky map. When the model identifies an obstruction with high confidence in a previously "clear" area, increment that bin's obstruction probability. This allows the map to adapt to new obstacles.

* **5.2. Develop Satellite Reliability Scoring:**
  * **Action:** When the ML model classifies an outage as `NETWORK_ISSUE` or `NO_SATELLITE_COVERAGE`, analyze the context.
  * **Logic:** Maintain reliability scores for sky *regions* (az/el bins), time-of-day, and, if possible, orbital planes. The score is a simple ratio of successful observations vs. those associated with network issues. We cannot track individual satellites, but this provides a powerful proxy.

* **5.3. Build the Predictive Engine:**
  * **Action:** Create a forward-looking service.
  * **Logic:**
        1. Get the predicted satellite paths for the next hour from the **Satellite Visibility Service**.
        2. Correlate these paths against the **Obstruction Sky Map** to identify passages through blocked areas.
        3. Correlate the paths against the **Reliability Scores** to identify passages through historically unreliable sky regions.
        4. Factor in the weather forecast.
        5. Generate a "risk score" for the upcoming hour and expose it via an API.

---

This master plan provides a robust, phased, and technically grounded path to achieving the sophisticated Starlink monitoring system we envision.
