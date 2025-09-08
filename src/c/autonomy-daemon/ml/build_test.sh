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

echo "Compiling test program..."
$CC $CFLAGS $INCLUDES -c test_ml_monitor.c -o test_ml_monitor.o
if [ $? -ne 0 ]; then
    echo "Error compiling test_ml_monitor.c"
    exit 1
fi

echo "Linking test executable..."
$CC test_ml_monitor.o ml_monitor.o ml_monitor_uci.o -o test_ml_monitor $LIBS
if [ $? -ne 0 ]; then
    echo "Error linking test executable"
    exit 1
fi

echo "Build successful!"
echo "Run './test_ml_monitor' to test the ML monitoring implementation"

# Clean up object files
rm -f *.o

echo "Build completed successfully!"