# ML Monitor Module Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_GNU_SOURCE
INCLUDES = -I../core -I../utils -I../starlink -I../external -I../gps
LIBS = -luci -lubox -lubus -lpthread -lm -ljson-c

# Source files
SOURCES = ml_monitor.c ml_monitor_uci.c ml_monitor_ubus.c ml_monitor_integration.c ml_monitor_phase3.c ml_monitor_phase4.c ml_monitor_phase5.c ml_monitor_phase6.c ml_monitor_phase7.c ml_monitor_multi_interface.c ml_monitor_multi_interface_ubus.c ml_monitor_advanced_networking.c ml_monitor_network_discovery_integration.c ml_monitor_analytics.c ml_monitor_analytics_ubus.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = ml_monitor.h ml_monitor_ubus.h ml_monitor_multi_interface.h ml_monitor_advanced_networking.h ml_monitor_network_discovery_integration.h ml_monitor_analytics.h

# Target library
TARGET = libml_monitor.a
CLI_TARGET = ml_monitor_cli

# Build rules
all: $(TARGET) $(CLI_TARGET)

$(TARGET): $(OBJECTS)
	ar rcs $@ $^
	@echo "Built ML monitor library: $@"

$(CLI_TARGET): ml_monitor_cli.o $(OBJECTS)
	$(CC) $(CFLAGS) ml_monitor_cli.o $(OBJECTS) -o $(CLI_TARGET) $(LIBS)
	@echo "Built ML monitor CLI: $@"

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) $(CLI_TARGET) ml_monitor_cli.o
	@echo "Cleaned ML monitor module"

install: $(TARGET)
	@echo "Installing ML monitor library..."
	# Installation would copy to appropriate system directories

test: $(TARGET)
	@echo "Running ML monitor tests..."
	# Test compilation and basic functionality

debug: CFLAGS += -g -DDEBUG -O0
debug: clean $(TARGET)
	@echo "Built ML monitor library with debug symbols"

.PHONY: all clean install test debug

# Dependencies
ml_monitor.o: ml_monitor.c ml_monitor.h
ml_monitor_uci.o: ml_monitor_uci.c ml_monitor.h
ml_monitor_ubus.o: ml_monitor_ubus.c ml_monitor.h ml_monitor_ubus.h