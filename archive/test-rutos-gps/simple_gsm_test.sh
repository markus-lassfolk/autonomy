#!/bin/bash

echo "🎯 Simple GSM GPS Test"
echo "====================="

echo ""
echo "📱 1. Modem Information:"
echo "  Model: $(gsmctl -m)"
echo "  Manufacturer: $(gsmctl -w)"
echo "  Firmware: $(gsmctl -y)"

echo ""
echo "🔋 2. GPS Power Commands:"
echo "  GPS Status: $(gsmctl -A 'AT+CGPS?')"
echo "  GPS Power: $(gsmctl -A 'AT+CGPSPWR?')"

echo ""
echo "📡 3. GPS Data Commands:"
echo "  CGPSINFO: $(gsmctl -A 'AT+CGPSINFO')"
echo "  CGNSINF: $(gsmctl -A 'AT+CGNSINF')"

echo ""
echo "🌐 4. Location Services:"
echo "  CLBS Status: $(gsmctl -A 'AT+CLBS=4,1')"

echo ""
echo "🔍 5. Alternative Commands:"
echo "  QGPS: $(gsmctl -A 'AT+QGPS?')"
echo "  QGPSLOC: $(gsmctl -A 'AT+QGPSLOC=2')"

echo ""
echo "✅ GSM GPS Test Complete"
