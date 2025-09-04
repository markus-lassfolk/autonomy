Starlink Obstruction Prediction System: Implementation Guide
Document Version: 1.1
Date: September 4, 2025
Audience: Developers, Program Managers

1. Executive Summary
1.1. Objective
This document outlines the architecture and implementation plan for a system to predict Starlink signal obstructions. The system will correlate the user terminal's (Dishy's) real-time, ground-truth obstruction map with high-precision satellite trajectory data from the space-track.org API. This will allow us to forecast potential outages, understand the impact of specific obstructions, and analyze overall link quality with unprecedented detail.

1.2. Feasibility & Business Value
The project is highly feasible. Your team's existing familiarity with the Starlink gRPC interface is a significant advantage. The necessary orbital data is programmatically accessible, and the scientific calculations are well-established.

Business Value:

Proactive Operations: Predict service degradation before it occurs, allowing for preemptive network traffic shaping or user notifications.

Optimized Deployments: Provide a tool to analyze potential installation sites by simulating satellite visibility against a site survey.

Advanced Troubleshooting: Differentiate between network issues, satellite-plane issues, and local physical obstructions with data-driven evidence.

2. System Architecture
The system is designed as a data-processing pipeline that ingests data from two independent sources, correlates them in a central engine, and exposes the results via an API for visualization or further analysis.


<img width="1048" height="719" alt="image" src="https://github.com/user-attachments/assets/e880a5e8-a370-46b5-acb8-2cdb071b0832" />


3.2. [Data Source 2] Acquiring Satellite Data from Space-Track.org
This API provides the authoritative TLE (Two-Line Element) sets we need for propagation.

Authentication: The API uses cookie-based authentication.

Perform a POST request to https://www.space-track.org/ajaxauth/login.

The request body should be identity=YOUR_USERNAME&password=YOUR_PASSWORD.

Store the returned Set-Cookie header. All subsequent API calls must include this cookie.

Fetching TLE Data:

Endpoint: https://www.space-track.org/basicspacedata/query/class/tle_latest/...

Query: Use a query to get the latest TLE for all operational Starlink satellites.

Recommended URL: /basicspacedata/query/class/tle_latest/OBJECT_NAME/~~STARLINK/ORDINAL/1/format/tle

This fetches the most recent (ORDINAL/1) TLE for all objects whose name contains "STARLINK".

Scheduling: This is not a real-time stream. TLEs are updated periodically. Implement a scheduled job to fetch and cache this data once every 12-24 hours. This respects their rate limits and is sufficient for accurate predictions.

3.3. The Propagation & Correlation Engine
This is the computational core of the system.

Recommended Libraries:
requests: For handling the Space-Track API calls and session management.
sgp4: The standard, high-performance Python implementation of the SGP4 propagator.
astropy: Absolutely essential for robust and accurate coordinate system transformations. Do not attempt these transformations manually.
numpy: For vectorized calculations to process thousands of satellites efficiently.

´´´
'''
Core Workflow (Python Example):

from sgp4.api import Satrec, jday
from astropy.coordinates import EarthLocation, TEME, ITRS, AltAz
from astropy.time import Time
from astropy import units as u

1. Get Observer Location from gRPC call `dish_get_status`
dish_status = # ... result of dish_get_status ...
lat, lon, alt = dish_status.location.lat_rads, dish_status.location.lon_rads, dish_status.location.altitude_msl
dish_location = EarthLocation(lat=lat*u.rad, lon=lon*u.rad, height=alt*u.m)

2. Load all Starlink TLEs from cached file into Satrec objects
tle_lines = open('starlink_tles.txt', 'r').readlines()
satellites = [Satrec.twoline2rv(l1, l2) for l1, l2 in zip(tle_lines[0::2], tle_lines[1::2])]

3. Create a time array for the prediction window (e.g., next 30 mins)
now = Time.now()
time_offsets_sec = np.arange(0, 1800, 5) # 5-second intervals for 30 mins
prediction_times = now + time_offsets_sec * u.s

4. Propagate all satellites at once (this can be parallelized)
all_obstructed_events = []
for satellite in satellites:
  sgp4 returns position in TEME (True Equator Mean Equinox) frame
  error, pos_teme_km, vel_teme_kms = satellite.sgp4_array(prediction_times.jd1, prediction_times.jd2)

5. Use Astropy for robust coordinate transformation
  teme_coords = TEME(pos_teme_km.T * u.km, representation_type='cartesian', obstime=prediction_times)
  itrs_coords = teme_coords.transform_to(ITRS(obstime=prediction_times))
  altaz_frame = AltAz(obstime=prediction_times, location=dish_location)
  local_sky_coords = itrs_coords.transform_to(altaz_frame)

Correlate with the obstruction map
  for i, time in enumerate(prediction_times):
  el = local_sky_coords.alt[i].deg
  if el >= MIN_ELEVATION_DEG:
  az = local_sky_coords.az[i].deg
  pixel = convert_az_el_to_pixel(az, el)
  if pixel and is_pixel_obstructed(pixel, obstruction_data_2d):
  all_obstructed_events.append({'time': time.iso, 'satellite_id': satellite.satnum, 'az': az, 'el': el})
'''
´´´

4. Advanced Features & Performance Optimization
Parallelization: The loop over satellites is "embarrassingly parallel." Use Python's multiprocessing library to distribute the propagation workload across all available CPU cores. This can yield a near-linear performance increase.

JIT Compilation: For ultimate performance, consider replacing the sgp4 library with a JIT-compiled propagator like poliastro (which uses numba) or heyoka.py. This can accelerate the propagation step by another order of magnitude, enabling near real-time, continuous prediction.

Data-Driven Obstruction Threshold: Instead of a binary "obstructed/not obstructed" flag, use the actual SNR values from the map. This allows for classifying obstructions as "marginal" vs. "critical" and correlating them with real-world packet loss data.

"Active Satellite" Heuristics: To reduce noise, cross-reference the status.data.currently_obstructed and status.data.seconds_to_first_non_obstructed_satellite gRPC fields. This helps identify which specific satellite the dish is likely trying to communicate with, allowing you to highlight its predicted path in the UI.

5. Visualization
The most intuitive way to present this data is a polar plot representing the sky view from the dish's perspective.

Background Layer: Render the 123x123 obstruction map as a color-meshed image (e.g., using matplotlib's pcolormesh or D3.js's equivalent). Use a color scale from green (high SNR) to red (low SNR).

Foreground Layer: Overlay the calculated paths of all satellites currently above the 25° elevation mask.

Color Coding:

Draw path segments in green where the path traverses a clear area.

Draw path segments in red where the path intersects a known obstruction.

Interactivity: Allow users to hover over a satellite path to see its NORAD ID and predicted time of arrival/departure from an obstructed zone.

Timeline View: Complement the sky view with a Gantt chart showing predicted obstruction windows for the next hour.
