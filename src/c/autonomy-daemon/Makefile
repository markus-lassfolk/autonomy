# Makefile for autonomy-daemon
# This Makefile is used during the build process to compile the autonomy-daemon

CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99 -O2
LDFLAGS ?= 
LIBS = -lubus -lblobmsg_json -lubox -ljson-c -luci -lcurl -lm -lpthread

# Source files
SOURCES = autonomy-daemon.c \
          logging.c \
          network.c \
          gps.c \
          config.c \
          pidfile.c \
          ubus_methods.c \
          network_ubus.c \
          gps_ubus.c \
          system_management.c \
          system_ubus.c

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