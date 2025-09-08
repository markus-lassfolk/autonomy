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

$CC test_ml_phase3.o ml_monitor.o ml_monitor_uci.o ml_monitor_integration.o ml_monitor_phase3.o -o test_ml_phase3 $LIBS
if [ $? -ne 0 ]; then
    echo "Error linking Phase 3 test executable"
    exit 1
fi

echo "Build successful!"
echo "Run './test_ml_monitor' to test basic ML monitoring"
echo "Run './test_ml_integration' to test Phase 2 integration" 
echo "Run './test_ml_phase3' to test Phase 3 advanced features"

# Clean up object files
rm -f *.o

echo "Build completed successfully!"