# Project Architecture: Predictive Multi-WAN Stability & Routing Engine

This document outlines the architecture for a dynamic WAN management system for a RUTX50 router. The system will monitor multiple diverse network interfaces, calculate a real-time **"Stability Score"** for each, and use these scores to intelligently manage routing priorities and failovers via MWAN3.

## 1. Core Principles & Design

The system will be built on four key components:

1.  **Modular Collectors:** Each connection type (Starlink, Cellular, etc.) will have its own dedicated script (`collector`) responsible for gathering its specific metrics. This makes the system clean and extensible.
2.  **Variable Polling Scheduler:** A central control loop will execute each collector at a configurable frequency, respecting the nature of the connection (e.g., frequent checks for Starlink, less frequent for metered cellular).
3.  **Unified Scoring Engine:** This component takes the raw data from all collectors, normalizes it, applies a weighted algorithm, and generates a single `Stability Score` (0-100) for each interface.
4.  **Action & Logging Engine:** This component reads the final scores, makes decisions (e.g., adjust MWAN3 priorities, log data), and persists all metrics for historical analysis.

![System Architecture Diagram](https://i.imgur.com/mOa87rC.png)

## 2. Metric Collection Framework

We will define two tiers of metrics for each connection.

-   **Common Metrics:** `ping_latency_ms`, `ping_loss_percent`, `jitter_ms`. These are the universal measures of connection quality.
-   **Specific Metrics:** Technology-specific data that provides deep insight into the connection's physical or virtual layer health.

| Connection Type  | Polling Interval | Common Metrics Tool | Specific Metrics & RUTOS Commands                                                                                                                   |
| :--------------- | :--------------- | :------------------ | :-------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Starlink** | 2 seconds        | `fping`             | **Source:** Starlink gRPC API <br> **Metrics:** `snr`, `popPingDropRate`, `fractionObstructed`, `secondsToFirstNonemptySlot` <br> **Command:** `grpcurl ... SpaceX.API.Device.Device/Handle` |
| **Cellular Modem** | 60 seconds       | `fping`             | **Source:** RUTOS `gsmctl` <br> **Metrics:** `rsrp`, `rsrq`, `sinr`, `rssi`, `cell_id`, `operator` <br> **Command:** `gsmctl -A 'AT+QCSQ;+COPS?;+QENG="servingcell"'` |
| **WiFi Bridge** | 15 seconds       | `fping`             | **Source:** RUTOS `iwinfo` <br> **Metrics:** `signal_rssi`, `noise_level`, `bitrate_mbps` <br> **Command:** `iwinfo <iface> info`                             |
| **LAN Connection** | 5 seconds        | `fping`             | **Source:** Linux Kernel (`/sys`) <br> **Metrics:** `link_state` (UP/DOWN), `link_speed_mbps` <br> **Command:** `cat /sys/class/net/<iface>/operstate` & `ethtool <iface>` |
| **VPN Tunnel** | 30 seconds       | `fping`             | **Source:** VPN Client status <br> **Metrics:** `tunnel_status` (UP/DOWN), `handshake_latency_ms` <br> **Command:** `wg show <iface> latest-handshakes` (WireGuard) |

## 3. The Stability Score Algorithm

The heart of the system is converting these diverse metrics into a single, comparable score.

### Step 1: Normalization

Every raw metric must be normalized to a standard scale of 0.0 to 1.0, where 1.0 is "best" and 0.0 is "worst".

-   **For metrics where higher is better (e.g., SNR, RSRP, Bitrate):**
    $Normalized = (Value - Worst\_Threshold) / (Best\_Threshold - Worst\_Threshold)$

-   **For metrics where lower is better (e.g., Latency, Obstruction, Noise):**
    $Normalized = 1 - ((Value - Best\_Threshold) / (Worst\_Threshold - Best\_Threshold))$

*Note: Values are clamped between 0.0 and 1.0 if they fall outside the threshold range.*

### Step 2: Weighting

Each normalized metric is assigned a weight reflecting its importance to overall connection quality.

### Step 3: Calculation

The final score is the weighted sum of the normalized metrics.

$Stability Score = 100 * ((w_1 * M_1) + (w_2 * M_2) + ...)$

#### Example: Cellular Stability Score Calculation

| Metric      | Raw Value | Thresholds (Worst/Best) | Normalized Value  | Weight (w) | Weighted Score |
| :---------- | :-------- | :---------------------- | :---------------- | :--------- | :------------- |
| ping_loss   | 25%       | 0 / 10                  | 0.0 (kill switch) | **KILL** | 0              |
| sinr        | 5 dB      | -5 / 20                 | 0.40              | 0.40       | 16.0           |
| rsrp        | -100 dBm  | -115 / -85              | 0.50              | 0.25       | 12.5           |
| latency     | 120 ms    | 300 / 40                | 0.69              | 0.20       | 13.8           |
| jitter      | 40 ms     | 150 / 5                 | 0.76              | 0.15       | 11.4           |
| **Total** |           |                         |                   | **1.00** | **Final Score = 53.7** |

*Note: A `ping_loss` over a critical threshold (e.g., 20%) should act as a "kill switch," immediately forcing the final score to 0.*

## 4. Prompt for Script Generation

This section provides a clear, structured prompt that can be used to guide a code-generating AI to build the scripts for this system.

---

### **AI Script Generation Prompt**

Please generate a set of `sh` scripts for a RUTX50 (BusyBox) router to create a predictive multi-WAN monitoring and scoring system based on the following architecture.

#### **Project Goal:**

The system must monitor multiple WAN interfaces (Starlink, Cellular, WiFi, LAN, VPN), collect common and specific metrics for each at different intervals, calculate a unified `Stability Score` (0-100) for each, and log the data.

#### **Script Logic Details:**

**1. `lib/common_functions.sh`**

-   Create a shell function `normalize_metric(value, best, worst, invert)` that takes a raw value and normalization parameters and echoes a normalized value between 0.0 and 1.0. The `invert` flag should be used for metrics where lower is better.
-   Create a logging function `log_message(level, message)` that prepends a timestamp and log level to messages.

**2. `collectors/*.sh` Scripts**

-   Each script should be responsible for ONE interface type.
-   It must gather all relevant metrics (common and specific) as defined in the architecture table. Use `fping` for common metrics and RUTOS-specific commands (`gsmctl`, `iwinfo`, `grpcurl`) for others.
-   The script must output its findings as a single line of JSON to stdout.
-   **Example output for `collect_cellular.sh`:**
    ```json
    {"iface": "cellular1", "timestamp": 1722984434, "type": "cellular", "metrics": {"ping_loss": 1.5, "latency": 88, "jitter": 22, "rsrp": -98, "sinr": 6}}
    ```

**3. `monitor.sh` (Main Controller)**

-   This script is the main loop (`while true; do ... done`).
-   It should maintain a timestamp for the last run of each collector.
-   Inside the loop, it checks if the elapsed time for each collector exceeds its configured polling interval (e.g., 60s for cellular, 2s for starlink).
-   If it's time to run a collector, it executes the corresponding `collect_*.sh` script.
-   It captures the JSON output from the collector and updates the central `data/latest_metrics.json` file.
-   After updating, it calls the `scoring/calculate_score.sh` script to re-calculate the scores.
-   It appends the new raw data to `data/history_log.csv`.

**4. `scoring/calculate_score.sh`**

-   This script reads the `data/latest_metrics.json` file.
-   For each interface, it applies the "Stability Score Algorithm":
    1.  It uses the `normalize_metric` function from the common library for each metric.
    2.  It applies the predefined weights for that interface type.
    3.  It implements a "kill switch" for high packet loss.
    4.  It calculates the final score (0-100).
-   It should update the `latest_metrics.json` file by adding a `"stability_score": XX` field to each interface's entry. This final JSON file is the "single source of truth" for the current state of all WAN connections.
