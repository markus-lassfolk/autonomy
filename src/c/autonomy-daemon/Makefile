# Makefile for autonomy-daemon
# This Makefile is used during the build process to compile the autonomy-daemon

CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99 -O2
LDFLAGS ?= 
LIBS = -lubus -lblobmsg_json -lubox -ljson-c -lcjson -luci -lcurl -lm -lpthread -lsqlite3 -lcrypto -lssl

# Source files - include all C files from subdirectories
SOURCES = $(shell find . -name "*.c" -not -name "test_*.c" -not -path "./testing/*" -not -path "./starlink/starlink_tracker.c" -not -path "./utils/cellular_collector.c" -not -path "./ubus/system_ubus.c" -not -path "./utils/system_management.c" -not -path "./network/network.c" -not -path "./gps/gps.c" -not -path "./utils/http_client.c" -not -name "network_discovery_simple.c" -not -name "gps_discovery_simple.c" -not -name "ml_monitor_cli.c")

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

# Linting and verification targets
lint-cppcheck:
	@echo "Running Cppcheck static analysis..."
	cppcheck --enable=all --inconclusive --std=c99 --suppress=missingIncludeSystem \
		--suppress=unusedFunction --suppress=unmatchedSuppression \
		--xml --xml-version=2 $(SOURCES) 2> cppcheck-report.xml || true
	@echo "Cppcheck analysis complete. Report saved to cppcheck-report.xml"

lint-clang-tidy:
	@echo "Running Clang-tidy analysis..."
	clang-tidy $(SOURCES) -- $(CFLAGS) -I. || true
	@echo "Clang-tidy analysis complete"

lint-splint:
	@echo "Running Splint analysis..."
	splint +posixlib -warnposix -preproc -nullret -nullassign -nullstate \
		$(SOURCES) || true
	@echo "Splint analysis complete"

lint-all: lint-cppcheck lint-clang-tidy lint-splint
	@echo "All linting tools completed"

verify-code:
	@echo "Running custom C code verification..."
	python3 ../../tools/verify_c_code.py --verbose .

# Phony targets
.PHONY: all clean install lint-cppcheck lint-clang-tidy lint-splint lint-all verify-code