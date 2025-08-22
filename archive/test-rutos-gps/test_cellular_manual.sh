#!/bin/bash

echo "🗼 Cellular Location Services Test"
echo "=================================="
echo ""

echo "📡 1. Testing Basic Location Services (CLBS):"
echo "  Command: gsmctl -A 'AT+CLBS=4,1'"
gsmctl -A 'AT+CLBS=4,1'
echo ""

echo "  Command: gsmctl -A 'AT+CLBS=2,1'"
gsmctl -A 'AT+CLBS=2,1'
echo ""

echo "🌐 2. Testing IP-Based Location (CIPGSMLOC):"
echo "  Command: gsmctl -A 'AT+CIPGSMLOC=1,1'"
gsmctl -A 'AT+CIPGSMLOC=1,1'
echo ""

echo "  Command: gsmctl -A 'AT+CIPGSMLOC=3,1'"
gsmctl -A 'AT+CIPGSMLOC=3,1'
echo ""

echo "📱 3. Testing Quectel Location Services (QLBS):"
echo "  Command: gsmctl -A 'AT+QLBS=2,1'"
gsmctl -A 'AT+QLBS=2,1'
echo ""

echo "🗼 4. Testing Cell-Based Location (QCELLLOC):"
echo "  Command: gsmctl -A 'AT+QCELLLOC=1,1'"
gsmctl -A 'AT+QCELLLOC=1,1'
echo ""

echo "📊 5. Cell Tower Information:"
echo "  Cell ID:"
gsmctl -C
echo ""

echo "  Network Info:"
gsmctl -F
echo ""

echo "  Signal Quality:"
gsmctl -q
echo ""

echo "✅ Cellular Location Test Complete!"
echo ""
echo "💡 Look for responses containing:"
echo "  +CLBS: location_type,longitude,latitude,accuracy,date,time"
echo "  +CIPGSMLOC: longitude,latitude,accuracy,date,time"
echo "  +QLBS: (Quectel location data)"
echo "  +QCELLLOC: (Cell location data)"
