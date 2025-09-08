#!/bin/bash

# Build script for ML Monitor test
echo "Building ML Monitor test..."

# Compiler and flags
CC=gcc
CFLAGS="-Wall -Wextra -O2 -std=c99 -D_GNU_SOURCE -DTEST_BUILD"
INCLUDES="-I../core -I../utils -I../starlink -I../external -I../gps"
LIBS="-lm -lpthread"

# Build the test
echo "Compiling ML Monitor modules..."
$CC $CFLAGS $INCLUDES -c ml_monitor.c -o ml_monitor.o
if [ $? -ne 0 ]; then
    echo "Error compiling ml_monitor.c"
    exit 1
fi

$CC $CFLAGS $INCLUDES -c ml_monitor_uci.c -o ml_monitor_uci.o
if [ $? -ne 0 ]; then
    echo "Error compiling ml_monitor_uci.c"
    exit 1
fi

echo "Compiling integration module..."
$CC $CFLAGS $INCLUDES -c ml_monitor_integration.c -o ml_monitor_integration.o
if [ $? -ne 0 ]; then
    echo "Error compiling ml_monitor_integration.c"
    exit 1
fi

echo "Compiling Phase 3 module..."
$CC $CFLAGS $INCLUDES -c ml_monitor_phase3.c -o ml_monitor_phase3.o
if [ $? -ne 0 ]; then
    echo "Error compiling ml_monitor_phase3.c"
    exit 1
fi

echo "Compiling Phase 4 module..."
$CC $CFLAGS $INCLUDES -c ml_monitor_phase4.c -o ml_monitor_phase4.o
if [ $? -ne 0 ]; then
    echo "Error compiling ml_monitor_phase4.c"
    exit 1
fi

echo "Compiling Phase 5 module..."
$CC $CFLAGS $INCLUDES -c ml_monitor_phase5.c -o ml_monitor_phase5.o
if [ $? -ne 0 ]; then
    echo "Error compiling ml_monitor_phase5.c"
    exit 1
fi

echo "Compiling Phase 6 module..."
$CC $CFLAGS $INCLUDES -c ml_monitor_phase6.c -o ml_monitor_phase6.o
if [ $? -ne 0 ]; then
    echo "Error compiling ml_monitor_phase6.c"
    exit 1
fi

echo "Compiling Phase 7 module..."
$CC $CFLAGS $INCLUDES -c ml_monitor_phase7.c -o ml_monitor_phase7.o
if [ $? -ne 0 ]; then
    echo "Error compiling ml_monitor_phase7.c"
    exit 1
fi

echo "Compiling Multi-Interface modules..."
$CC $CFLAGS $INCLUDES -c ml_monitor_multi_interface.c -o ml_monitor_multi_interface.o
if [ $? -ne 0 ]; then
    echo "Error compiling ml_monitor_multi_interface.c"
    exit 1
fi

$CC $CFLAGS $INCLUDES -c ml_monitor_multi_interface_ubus.c -o ml_monitor_multi_interface_ubus.o
if [ $? -ne 0 ]; then
    echo "Error compiling ml_monitor_multi_interface_ubus.c"
    exit 1
fi

echo "Compiling test programs..."
$CC $CFLAGS $INCLUDES -c test_ml_monitor.c -o test_ml_monitor.o
if [ $? -ne 0 ]; then
    echo "Error compiling test_ml_monitor.c"
    exit 1
fi

$CC $CFLAGS $INCLUDES -c test_ml_integration.c -o test_ml_integration.o
if [ $? -ne 0 ]; then
    echo "Error compiling test_ml_integration.c"
    exit 1
fi

$CC $CFLAGS $INCLUDES -c test_ml_phase3.c -o test_ml_phase3.o
if [ $? -ne 0 ]; then
    echo "Error compiling test_ml_phase3.c"
    exit 1
fi

$CC $CFLAGS $INCLUDES -c test_ml_phase4.c -o test_ml_phase4.o
if [ $? -ne 0 ]; then
    echo "Error compiling test_ml_phase4.c"
    exit 1
fi

$CC $CFLAGS $INCLUDES -c test_ml_phase5.c -o test_ml_phase5.o
if [ $? -ne 0 ]; then
    echo "Error compiling test_ml_phase5.c"
    exit 1
fi

$CC $CFLAGS $INCLUDES -c test_ml_phase6.c -o test_ml_phase6.o
if [ $? -ne 0 ]; then
    echo "Error compiling test_ml_phase6.c"
    exit 1
fi

$CC $CFLAGS $INCLUDES -c test_ml_phase7.c -o test_ml_phase7.o
if [ $? -ne 0 ]; then
    echo "Error compiling test_ml_phase7.c"
    exit 1
fi

$CC $CFLAGS $INCLUDES -c test_ml_multi_interface.c -o test_ml_multi_interface.o
if [ $? -ne 0 ]; then
    echo "Error compiling test_ml_multi_interface.c"
    exit 1
fi

echo "Linking test executables..."
$CC test_ml_monitor.o ml_monitor.o ml_monitor_uci.o -o test_ml_monitor $LIBS
if [ $? -ne 0 ]; then
    echo "Error linking basic test executable"
    exit 1
fi

$CC test_ml_integration.o ml_monitor.o ml_monitor_uci.o ml_monitor_integration.o ml_monitor_phase3.o -o test_ml_integration $LIBS
if [ $? -ne 0 ]; then
    echo "Error linking integration test executable"
    exit 1
fi

$CC test_ml_phase3.o ml_monitor.o ml_monitor_uci.o ml_monitor_integration.o ml_monitor_phase3.o ml_monitor_phase4.o -o test_ml_phase3 $LIBS
if [ $? -ne 0 ]; then
    echo "Error linking Phase 3 test executable"
    exit 1
fi

$CC test_ml_phase4.o ml_monitor.o ml_monitor_uci.o ml_monitor_integration.o ml_monitor_phase3.o ml_monitor_phase4.o ml_monitor_phase5.o -o test_ml_phase4 $LIBS
if [ $? -ne 0 ]; then
    echo "Error linking Phase 4 test executable"
    exit 1
fi

ALL_MODULES="ml_monitor.o ml_monitor_uci.o ml_monitor_integration.o ml_monitor_phase3.o ml_monitor_phase4.o ml_monitor_phase5.o ml_monitor_phase6.o ml_monitor_phase7.o ml_monitor_multi_interface.o ml_monitor_multi_interface_ubus.o"

$CC test_ml_phase5.o $ALL_MODULES -o test_ml_phase5 $LIBS
if [ $? -ne 0 ]; then
    echo "Error linking Phase 5 test executable"
    exit 1
fi

$CC test_ml_phase6.o $ALL_MODULES -o test_ml_phase6 $LIBS
if [ $? -ne 0 ]; then
    echo "Error linking Phase 6 test executable"
    exit 1
fi

$CC test_ml_phase7.o $ALL_MODULES -o test_ml_phase7 $LIBS
if [ $? -ne 0 ]; then
    echo "Error linking Phase 7 test executable"
    exit 1
fi

$CC test_ml_multi_interface.o $ALL_MODULES -o test_ml_multi_interface $LIBS
if [ $? -ne 0 ]; then
    echo "Error linking Multi-Interface test executable"
    exit 1
fi

echo "Build successful!"
echo "Run './test_ml_monitor' to test basic ML monitoring"
echo "Run './test_ml_integration' to test Phase 2 integration" 
echo "Run './test_ml_phase3' to test Phase 3 advanced features"
echo "Run './test_ml_phase4' to test Phase 4 ensemble methods"
echo "Run './test_ml_phase5' to test Phase 5 mobile optimization"
echo "Run './test_ml_phase6' to test Phase 6 self-optimization"
echo "Run './test_ml_phase7' to test Phase 7 multi-interface intelligence (COMPLETE SYSTEM)"
echo "Run './test_ml_multi_interface' to test multi-interface system specifically"

# Clean up object files
rm -f *.o

echo "Build completed successfully!"