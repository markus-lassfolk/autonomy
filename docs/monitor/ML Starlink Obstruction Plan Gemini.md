# Gemini

## Analysis of Existing System

1. **Core Logic:** The main operational logic is in `src/c/autonomy-daemon/`. This is a C-based
   daemon that handles the low-level interactions with hardware and network services.
2. **Starlink Integration:** We have a dedicated `starlink/` module within the daemon. It already contains logic for:
    * **Obstruction Monitoring:** `starlink_obstruction.c`
    * **General Tracking/Data Collection:** `starlink_tracker.c`, `starlink_collector.c`,
      `starlink_comprehensive.c`
    * This confirms we are actively pulling data from the Starlink dish.
3. **GPS & Location:** There is a very mature `gps/` module. It handles:
    * GPS data parsing, location management, and even weather data (`gps_weather.c`).
    * Crucially, `gps_starlink.c` indicates a direct link between the GPS and Starlink modules,
      which is perfect for our needs.
4. **Satellite Tracking:** I haven't found any evidence of an *external* satellite tracking
   integration (e.g., pulling data from CelesTrak). The term "tracker" in `starlink_tracker.c`
   likely refers to tracking the *state* of the Starlink dish itself, not the positions of
   satellites in orbit. **This is the primary missing piece.**
5. **API Data Available:** The API documentation confirms we can get all the critical data points we need from the dish:
    * **Real-time Obstruction:** `obstructionStats` (including `fractionObstructed` and detailed `wedgeFractionObstructed`).
    * **Outage History:** The `get_history` method provides arrays of historical data for `popPingDropRate`, `popPingLatencyMs`, and `obstructed`. This is a goldmine for our analysis.
    * **Dish Pointing:** `boresightAzimuthDeg` and `boresightElevationDeg` tell us exactly where the dish is looking at any given moment.
    * **Precise Location:** We can get latitude, longitude, and altitude from the dish's own GPS.

## The Path Forward: A Detailed Plan

We have a fantastic foundation. The daemon is already collecting most of the necessary data from the dish. The main gap is correlating this data with the actual positions of satellites in the sky.

Here is a phased plan to build the ML-powered monitoring system you envision, building upon our existing architecture.

---

### Phase 1: Establish the "Ground Truth" - Satellite Tracking

The first step is to build a service that knows where every Starlink satellite is at any given time. This service will be the core of our contextual analysis.

* **1.1: Create a Satellite Data Ingestion Service:**
  * **Action:** Develop a new service (Python would be an excellent choice for this due to its strong data science and astronomy libraries) that periodically fetches the latest Starlink satellite orbital data (TLEs - Two-Line Elements) from a public source like CelesTrak.
  * **Tools:** Use libraries like `requests` to fetch the data and schedule it to run every few hours to keep the data fresh.

* **1.2: Implement a Satellite Position Calculation API:**
  * **Action:** Using the downloaded TLE data, build an internal API endpoint. This API will accept a `timestamp` and our `latitude/longitude` as input and return a list of all Starlink satellites that were visible from our location at that exact time. For each satellite, it should return its `azimuth`, `elevation`, and a unique `ID`.
  * **Tools:** The `skyfield` Python library is the industry standard for this. It is highly accurate and efficient.

---

### Phase 2: Data Enrichment and Storage

Now we need to connect the new satellite position data with the data we're already collecting from the Starlink daemon.

* **2.1: Enhance the Starlink Data Collector:**
  * **Action:** Modify our existing `starlink_collector.c` (or a related module) to do more than just store the raw `get_history` data. For each outage or significant performance event (high latency, packet loss), it needs to capture a complete snapshot.
  * **Snapshot Data:** `timestamp`, `duration`, `outage_type` (from Starlink), `dish_azimuth`, `dish_elevation`, `snr`, `popPingDropRate`, etc.

* **2.2: Create a Centralized Time-Series Database:**
  * **Action:** Set up a time-series database to store the enriched data. This is crucial for handling the volume and nature of the data we'll be collecting.
  * **Tools:** `InfluxDB` or `TimescaleDB` (a PostgreSQL extension) are excellent choices.
  * **Schema:** The database schema should be designed to hold the combined "snapshot" data: the dish's status *and* the list of visible satellites at that moment.

* **2.3: Build the Correlation Service:**
  * **Action:** Create a new service that reads the raw outage events from the daemon, calls the new Satellite Position API (from 1.2) to get the list of visible satellites for the event's timestamp, and then writes this fully enriched data point into the time-series database.

---

### Phase 3: Analysis, Visualization, and Initial ML Modeling

With data flowing, we can start analyzing it and building our first models.

* **3.1: Develop a Visualization Dashboard:**
  * **Action:** Create a web-based dashboard to visualize the data. This is essential for understanding the patterns yourself before asking the ML to do it.
  * **Views:**
        1. **Sky Map View:** A polar plot showing the dish's obstruction map. Overlay dots representing the locations of historical outages, color-coded by severity. You could even animate this over time.
        2. **Timeline View:** A graph showing latency, packet loss, and SNR over time. Mark the points where outages occurred.
        3. **Satellite View:** Select a specific outage event and see the list of satellites that were in the field of view, along with the dish's pointing direction at that time.
  * **Tools:** `Grafana` (which integrates well with InfluxDB/TimescaleDB) or a custom web app using a library like `D3.js`.

* **3.2: Data Labeling and First-Pass ML Model:**
  * **Action:** Using the dashboard, manually analyze and label a few hundred outage events. The initial labels would be simple: `Obstruction`, `No Satellites Visible`, `Network Issue`.
  * **Action:** Train a basic classification model (like a `Random Forest` or `Gradient Boosting` model) to automatically categorize new outages based on the features in the data snapshot. The goal is to see if the model can reliably distinguish a clear obstruction from a situation where no satellites were available.
  * **Tools:** Python with `scikit-learn`.

---

### Phase 4: Advanced ML and Automation

Now we refine the system to achieve your full vision.

* **4.1: Implement the Self-Learning Obstruction Map:**
  * **Action:** Develop the logic where, if the ML model classifies an outage as `Obstruction` with high confidence, but the location is in a "clear" area of the saved map, the system adds a "potential obstruction" point to a new map layer. Over time, these points build a more accurate, real-world map.

* **4.2: Track Individual Satellite Reliability:**
  * **Action:** When the model classifies an outage as a `Network Issue`, log the IDs of the visible satellites at that moment. Create a scoring system to track the reliability of each individual satellite over time. This could be as simple as a counter for "associated outages."

* **4.3: Build a Predictive Engine:**
  * **Action:** Use the satellite position service to look ahead. By knowing the future paths of satellites, the system could predict potential "coverage gaps" where no satellites will be visible in the unobstructed parts of your sky, or warn when a satellite with a historically poor reliability score is about to become your primary connection.

This plan builds logically from our current capabilities, addresses the key missing piece (satellite tracking), and moves progressively towards the intelligent, self-learning system you've described.

What are your thoughts on this plan? Does Phase 1 seem like a good place to start? I will create a todo list to track our progress.

[1 tool called]

Excellent. I've laid out a comprehensive analysis and a detailed plan to bring your vision for an intelligent Starlink monitoring system to life. The roadmap is broken down into four distinct phases, starting with the foundational step of integrating real-time satellite tracking and progressing all the way to a predictive, self-learning system.

I've captured this entire plan in our task list to ensure we can track our progress effectively.

I'm ready to proceed whenever you are. Please let me know your thoughts on the plan, and if you agree that we should begin with **Phase 1**.
