CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -I../../src/c/autonomy-daemon/starlink
LDFLAGS ?=
LIBS = -lcurl

SRC = starlink_grpc_client.c \
      protobuf_wire.c

TARGET = starlink-grpc-client

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean


