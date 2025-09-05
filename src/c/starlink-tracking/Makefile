# Makefile for starlink-tracking
# This Makefile is used during the build process to compile the starlink-tracking

CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99 -O2
LDFLAGS ?= 
LIBS = -lcurl -ljson-c -lubox -lubus -luci -lm -lpthread

# Source files
SOURCES = starlink_client.c \
          starlink_collector.c \
          starlink_ubus.c \
          starlink_cluster.c \
          starlink_cluster_ubus.c \
          starlink_tracker.c \
          starlink_tracker_ubus.c \
          space_track_connector.c \
          obstruction_analyzer.c \
          obstruction_analyzer_enhanced.c \
          prediction_engine.c \
          validation_module.c \
          astro_coordinates.c \
          dynamic_satellite_tracker.c \
          parallel_propagator.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Target executable
TARGET = starlink-tracker

# Default target
all: $(TARGET)

# Build the main executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS) $(LIBS)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Install the executable
install: $(TARGET)
	install -d $(DESTDIR)/usr/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/bin/

# Phony targets
.PHONY: all clean install
