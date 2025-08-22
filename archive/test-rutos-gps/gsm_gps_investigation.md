# GSM GPS Investigation Guide

## 🎯 Goal: Get GSM GPS Working as Tertiary Option

Based on our testing, the GSM AT commands were returning "ERROR". Let's investigate systematically:

## 📱 Step 1: Check Modem Capabilities

Run these commands on RutOS to identify the modem:

```bash
# Get modem information
gsmctl -m    # Model
gsmctl -w    # Manufacturer  
gsmctl -y    # Firmware
gsmctl -i    # IMEI

# Check available AT commands
gsmctl -A 'AT+CLAC' | grep -i gps
```

## 🔋 Step 2: GPS Power Management

Different modems use different GPS power commands:

```bash
# Standard GPS power commands
gsmctl -A 'AT+CGPS?'      # Check GPS status
gsmctl -A 'AT+CGPSPWR?'   # Check GPS power
gsmctl -A 'AT+CGPS=1'     # Turn on GPS
gsmctl -A 'AT+CGPSPWR=1'  # Power on GPS

# Quectel-specific commands (if Quectel modem)
gsmctl -A 'AT+QGPS=1'     # Quectel GPS on
gsmctl -A 'AT+QGPSGNMEA=1' # Enable NMEA

# u-blox commands (if u-blox modem)
gsmctl -A 'AT+UGPS=1,1,1' # u-blox GPS on
```

## 📡 Step 3: GPS Data Retrieval

Try these commands in order:

```bash
# Standard GPS info commands
gsmctl -A 'AT+CGPSINFO'   # GPS information
gsmctl -A 'AT+CGNSINF'    # GNSS information

# Location services
gsmctl -A 'AT+CLBS=1,1'   # Enable location services
gsmctl -A 'AT+CLBS=4,1'   # Get current location

# Quectel-specific (if applicable)
gsmctl -A 'AT+QGPSLOC=2'  # Quectel location
gsmctl -A 'AT+QGPSGNMEA?' # Get NMEA data

# Alternative location methods
gsmctl -A 'AT+CIPGSMLOC=1,1' # GSM location
gsmctl -A 'AT+CIPGSMLOC=2,1' # GPS location
```

## 🔍 Step 4: Troubleshooting

If commands return ERROR, try:

1. **Check GPS antenna connection**
2. **Enable GPS power first**
3. **Wait for GPS fix (can take 30-60 seconds)**
4. **Try modem-specific commands**

## 📊 Expected Output Formats

### CGPSINFO Response:
```
+CGPSINFO: lat,N,lon,E,date,time,alt,speed,course
Example: +CGPSINFO: 5928.804065,N,01816.791257,E,150825,231400.00,9.6,0.0,0.0
```

### CGNSINF Response:
```
+CGNSINF: status,lat,lon,alt,speed,course,fix_mode,reserved1,HDOP,PDOP,VDOP,reserved2,satellites,reserved3,reserved4
Example: +CGNSINF: 1,59.480068,18.279854,9.6,0.0,0.0,1,,1.2,1.8,1.0,,10,,
```

### CLBS Response:
```
+CLBS: location_type,longitude,latitude,accuracy,date,time
Example: +CLBS: 0,18.279854,59.480068,100,25/08/15,23:14:00
```

## 🎯 Implementation Strategy

Once we identify working commands:

1. **Primary**: External GPS Antenna (`gpsctl`)
2. **Secondary**: Starlink GPS (API)  
3. **Tertiary**: GSM GPS (working AT commands)

## 🔧 Next Steps

1. Run the investigation commands manually
2. Identify which commands work for your specific modem
3. Update the GPS collector with working GSM commands
4. Implement proper error handling and fallback logic

## 📝 Notes

- GSM GPS accuracy: typically 50-500 meters
- Requires cellular signal for location services
- May need GPS antenna for satellite-based GPS
- Some modems support both GSM location and GPS satellites
