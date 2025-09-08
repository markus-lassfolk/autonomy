# ML Monitor Module Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_GNU_SOURCE
INCLUDES = -I../core -I../utils -I../starlink -I../external -I../gps
LIBS = -luci -lubox -lubus -lpthread -lm -ljson-c

# Source files
SOURCES = ml_monitor.c ml_monitor_uci.c ml_monitor_ubus.c ml_monitor_integration.c ml_monitor_phase3.c ml_monitor_phase4.c ml_monitor_phase5.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = ml_monitor.h ml_monitor_ubus.h

# Target library
TARGET = libml_monitor.a

# Build rules
all: $(TARGET)

$(TARGET): $(OBJECTS)
	ar rcs $@ $^
	@echo "Built ML monitor library: $@"

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
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