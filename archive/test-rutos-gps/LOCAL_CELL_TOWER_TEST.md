# 🎯 Local Cell Tower Location Test

## 🚀 **Quick Start - Run Locally Without SSH**

Test both Mozilla Location Service and OpenCellID using your hardcoded cell tower data:

```bash
go run *.go -test-cell-local
```

## 📊 **What This Test Uses:**

### **📍 GPS Reference (Hardcoded)**
```
Latitude:  59.48007000°
Longitude: 18.27985000°
Accuracy:  ±0.4 meters
Source:    Your Quectel Multi-GNSS data
```

### **📡 Cell Tower Data (Hardcoded from Your RutOS)**
```
Cell ID:    25939743 (hex: 18BCF1F)
Network:    Telia Sweden (MCC:240, MNC:01)
Technology: 5G-NSA, LTE Band B3
Signal:     RSSI -53 (Excellent)
PCID:       443
TAC:        23
EARFCN:     1300
```

### **🗼 Neighbor Cells (Simulated)**
```
PCID 444: RSSI -67 (intra-frequency)
PCID 445: RSSI -72 (intra-frequency)  
PCID 446: RSSI -78 (intra-frequency)
PCID 447: RSSI -81 (inter-frequency)
```

## 🔧 **Requirements:**

1. **OpenCellID API Key:** `C:\Users\markusla\OneDrive\IT\RUTOS Keys\OpenCELLID.txt`
2. **Internet Connection:** To reach Mozilla and OpenCellID APIs
3. **No SSH Required:** Runs completely locally

## 📊 **Expected Output:**

```
🎯 LOCAL CELL TOWER LOCATION ACCURACY TEST
=============================================
📡 Using hardcoded cell tower data from your RutOS device

📍 GPS Reference: 59.48007000°, 18.27985000° (±0.4m)
📡 Cell Tower: 25939743 (Telia Sweden, MCC:240, MNC:01)
📊 Signal: RSSI -53, RSRP -84, RSRQ -8

🦊 Testing Mozilla Location Service...
-----------------------------------
  📡 Sending request to Mozilla Location Service...
  📊 Using 5 cell towers for triangulation
  📡 Response: {"location":{"lat":59.4801,"lng":18.2799},"accuracy":500}
  ✅ Mozilla SUCCESS: 59.480100°, 18.279900° (±500m) in 234.5ms
  🗺️  Mozilla Maps: https://www.google.com/maps?q=59.480100,18.279900

🗼 Testing OpenCellID Service...
------------------------------
  📡 Sending request to OpenCellID...
  📊 Looking up Cell ID: 25939743 (MCC:240, MNC:1, LAC:23)
  📡 Response: {"status":"ok","lat":59.4801,"lon":18.2799,"accuracy":200}
  ✅ OpenCellID SUCCESS: 59.480100°, 18.279900° (±200m) in 567.2ms
  🗺️  OpenCellID Maps: https://www.google.com/maps?q=59.480100,18.279900

📊 COMPARISON RESULTS
=========================

📍 GPS Reference: 59.48007000°, 18.27985000° (±0.4m)
🦊 Mozilla: 59.480100°, 18.279900° → 347m from GPS
🗼 OpenCellID: 59.480100°, 18.279900° → 156m from GPS

🏆 WINNER:
   🗼 OpenCellID (191m more accurate)

💡 RECOMMENDATION:
   ✅ Both services are accurate enough for location fallback
   🆓 Use Mozilla for free production deployment
   🎯 Use OpenCellID for higher accuracy needs

🗺️  Compare all locations on map:
   GPS: https://www.google.com/maps?q=59.48007000,18.27985000
   Mozilla: https://www.google.com/maps?q=59.480100,18.279900
   OpenCellID: https://www.google.com/maps?q=59.480100,18.279900

💾 Results saved to: local_cell_tower_test_2024-12-24_15-30-45.json
```

## 🎯 **Benefits of This Test:**

### **✅ Advantages:**
- **No SSH Required** - Runs completely locally
- **Real Cell Tower Data** - Uses your actual RutOS cell info
- **Real API Responses** - Tests actual Mozilla and OpenCellID services
- **Accurate Comparison** - Compares against your precise GPS coordinates
- **Fast Execution** - No need to connect to RutOS device

### **📊 What You'll Learn:**
1. **Which service is more accurate** for your specific cell tower
2. **Actual response times** from both services
3. **Whether cell tower location is viable** as GPS fallback
4. **How far off** cell tower location is from GPS truth

## 🔧 **Troubleshooting:**

### **OpenCellID API Key Issues:**
```bash
# Check if file exists
ls "C:\Users\markusla\OneDrive\IT\RUTOS Keys\OpenCELLID.txt"

# Check file contents (should be just the token)
type "C:\Users\markusla\OneDrive\IT\RUTOS Keys\OpenCELLID.txt"
```

### **Network Issues:**
- Ensure internet connectivity
- Check if corporate firewall blocks APIs
- Try with VPN if needed

### **Compilation Issues:**
```bash
# Build first to check for errors
go build -o test-cell-local *.go

# Then run
./test-cell-local -test-cell-local
```

## 🎉 **Perfect for Testing:**

This local test is **perfect** for:
- **Quick validation** of cell tower location services
- **Comparing accuracy** without SSH complexity  
- **Testing API connectivity** and response times
- **Evaluating** which service works best for your area
- **Development** and integration testing

**Run it now to see how accurate cell tower location can be for your specific location!** 🛰️🗼📍
