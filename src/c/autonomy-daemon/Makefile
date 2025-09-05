# Makefile for autonomy-daemon
# This Makefile is used during the build process to compile the autonomy-daemon

CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99 -O2
LDFLAGS ?= 
LIBS = -lubus -lblobmsg_json -lubox -ljson-c -luci -lcurl -lm -lpthread

# Source files
SOURCES = autonomy-daemon.c \
          modules/logging.c \
          modules/network.c \
          modules/gps.c \
          modules/config.c \
          modules/pidfile.c \
          modules/ubus_methods.c \
          modules/network_ubus.c \
          modules/gps_ubus.c \
          modules/system_management.c \
          modules/system_ubus.c \
          modules/starlink/starlink_client.c \
          modules/starlink/starlink_collector.c \
          modules/starlink/starlink_ubus.c \
          modules/starlink/starlink_cluster.c \
          modules/starlink/starlink_cluster_ubus.c \
          modules/starlink/starlink_tracker.c \
          modules/starlink/starlink_tracker_ubus.c \
          modules/starlink/space_track_connector.c \
          modules/starlink/obstruction_analyzer.c \
          modules/starlink/prediction_engine.c \
          modules/starlink/validation_module.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Target executable
TARGET = autonomy-daemon

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