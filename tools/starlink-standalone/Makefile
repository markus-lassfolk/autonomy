CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -I../../src/c/autonomy-daemon/starlink
LDFLAGS ?=
LIBS = -lcurl

SRC = starlink_grpc_standalone.c \
      ../../src/c/autonomy-daemon/starlink/protobuf_wire.c

TARGET = starlink-grpc-client

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean


