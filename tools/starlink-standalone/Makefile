# Makefile for Starlink Standalone gRPC Client
# This builds the standalone client using the shared libraries from the daemon

CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99 -O2 -g -rdynamic
LDFLAGS ?= -rdynamic
LIBS = -lcurl -lm -ljson-c

# Source files
SOURCES = starlink_grpc_client.c protobuf_wire.c
SHARED_SOURCES = ../../src/c/autonomy-daemon/shared/protobuf/protobuf_wire.c \
                 ../../src/c/autonomy-daemon/shared/starlink/starlink_grpc_shared.c

# Object files
OBJECTS = $(SOURCES:.c=.o)
SHARED_OBJECTS = $(SHARED_SOURCES:.c=.o)

# Target executable
TARGET = starlink-grpc-client

# Include directories
INCLUDES = -I. -I../../src/c/autonomy-daemon/shared/protobuf -I../../src/c/autonomy-daemon/shared/starlink -I../../src/c/autonomy-daemon/shared/logging

# Default target
all: $(TARGET)

# Build the main executable
$(TARGET): $(OBJECTS) $(SHARED_OBJECTS)
	$(CC) $(OBJECTS) $(SHARED_OBJECTS) -o $(TARGET) $(LDFLAGS) $(LIBS)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Compile shared source files
../../src/c/autonomy-daemon/shared/protobuf/%.o: ../../src/c/autonomy-daemon/shared/protobuf/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

../../src/c/autonomy-daemon/shared/starlink/%.o: ../../src/c/autonomy-daemon/shared/starlink/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(SHARED_OBJECTS) $(TARGET)

# Install the executable
install: $(TARGET)
	install -d $(DESTDIR)/usr/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/bin/

# Test the client
test: $(TARGET)
	@echo "Testing Starlink gRPC client..."
	@echo "Usage: ./$(TARGET) <host:port> <method>"
	@echo "Example: ./$(TARGET) 192.168.100.1:9200 get_status"

.PHONY: all clean install test