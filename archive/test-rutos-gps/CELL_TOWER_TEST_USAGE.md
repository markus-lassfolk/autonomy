# 🎯 Cell Tower Location Accuracy Test

## 🚀 **Quick Start**

Test both Mozilla Location Service and OpenCellID against your super accurate GPS data:

```bash
go run *.go -test-cell-accuracy
```

## 📋 **What This Test Does:**

### **Step 1: GPS Reference** 📍
- Gets your most accurate GPS coordinates (Quectel Multi-GNSS preferred)
- Uses this as the "ground truth" for comparison
- Expected accuracy: ~0.4 meters with 37 satellites

### **Step 2: Cellular Data Collection** 📡
- Collects comprehensive cellular network information
- Your current cell: **Cell ID 25939743** (Telia Sweden)
- Gathers neighbor cells for triangulation
- Collects signal strength (RSSI, RSRP, RSRQ, SINR)

### **Step 3: Mozilla Location Service Test** 🦊
- **FREE service, no API key required**
- Uses your cell tower + neighbor cells for triangulation
- Expected accuracy: 100-2000 meters
- Response time: Usually < 500ms

### **Step 4: OpenCellID Test** 🗼
- **Uses your API key from:** `C:\Users\markusla\OneDrive\IT\RUTOS Keys\OpenCELLID.txt`
- More accurate database (40+ million towers)
- Expected accuracy: 50-1000 meters
- Response time: Usually < 1000ms

### **Step 5: Comparison & Analysis** 📊
- Calculates distance from GPS reference to each service result
- Determines which service is more accurate for your location
- Provides recommendations for production use

## 📊 **Expected Results:**

Based on your location data, you should see something like:

```
🎯 COMPREHENSIVE CELL TOWER LOCATION TEST RESULTS
============================================================

📍 GPS Reference Location:
  Coordinates: 59.48007000°, 18.27985000°
  Accuracy: ±0.4 meters
  Source: quectel_gsm_gps

📡 Cellular Data Summary:
  Cell ID: 25939743
  Network: Telia 5G-NSA (MCC:240, MNC:01)
  Signal: RSSI -53, RSRP -84, RSRQ -8
  Neighbor Cells: 5 detected

🦊 Mozilla Location Service Results:
  ✅ SUCCESS
  Coordinates: 59.480123°, 18.279834°
  Claimed Accuracy: ±500 meters
  Actual Accuracy: 347 meters from GPS
  Response Time: 234.5 ms
  Method: multi_cell_triangulation

🗼 OpenCellID Results:
  ✅ SUCCESS
  Coordinates: 59.480089°, 18.279851°
  Claimed Accuracy: ±200 meters
  Actual Accuracy: 156 meters from GPS
  Response Time: 567.2 ms
  Method: enhanced_lookup

📊 Service Comparison:
  Winner: OpenCellID
  Accuracy Difference: 191 meters
  Recommendation: OpenCellID (more accurate)
  Summary: Mozilla: 347m, OpenCellID: 156m accuracy. OpenCellID is 191m more accurate.

🗺️  Google Maps Links:
  GPS Reference: https://www.google.com/maps?q=59.48007000,18.27985000
  Mozilla Result: https://www.google.com/maps?q=59.480123,18.279834
  OpenCellID Result: https://www.google.com/maps?q=59.480089,18.279851
```

## 💾 **Output Files:**

The test automatically saves detailed results to:
```
cell_tower_test_2024-12-24_15-30-45.json
```

This JSON file contains:
- Complete test parameters
- Raw cellular data
- Both service responses
- Detailed accuracy analysis
- Timestamps and performance metrics

## 🎯 **Use Cases for Your autonomy System:**

### **1. Indoor GPS Fallback**
```go
if gpsSignalLost() {
    cellLocation := getCellTowerLocation()
    if cellLocation.Accuracy < 500 {
        return cellLocation  // Good enough for area detection
    }
}
```

### **2. GPS Validation**
```go
distance := calculateDistance(gpsLocation, cellLocation)
if distance > 5000 {  // 5km difference
    log.Warn("GPS may be spoofed or inaccurate")
}
```

### **3. Geofencing**
```go
if cellLocation.Accuracy < 1000 {
    if isWithinArea(cellLocation, homeArea) {
        return "home"
    }
}
```

## 🔧 **Troubleshooting:**

### **OpenCellID API Key Issues:**
- Ensure `C:\Users\markusla\OneDrive\IT\RUTOS Keys\OpenCELLID.txt` exists
- File should contain only your API token (no extra spaces/newlines)
- Get free API key at: https://opencellid.org/

### **No GPS Reference:**
- Make sure GPS is working: `go run *.go -test-quectel`
- Try enhanced GPS: `go run *.go -enhanced`
- Check GPS antenna connection

### **Cellular Data Issues:**
- Verify modem connection: `gsmctl -q`
- Check network registration: `gsmctl -A 'AT+CREG?'`
- Ensure good signal strength (RSSI > -100)

## 🎉 **Expected Outcome:**

This comprehensive test will show you:

1. **Which service works better for your specific location**
2. **How accurate cell tower location can be** (typically 100-500m)
3. **Response times for each service**
4. **Whether cell tower location is viable as a GPS fallback**

**Perfect for determining your fourth location source in the autonomy system!** 🛰️🗼📍
