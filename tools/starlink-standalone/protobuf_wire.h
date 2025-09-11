#ifndef PROTOBUF_WIRE_H
#define PROTOBUF_WIRE_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    PB_WIRE_VARINT = 0,
    PB_WIRE_64BIT = 1,
    PB_WIRE_LEN = 2,
    PB_WIRE_START_GROUP = 3,
    PB_WIRE_END_GROUP = 4,
    PB_WIRE_32BIT = 5
} pb_wire_type_t;

typedef struct {
    const unsigned char *data;
    size_t length;
    size_t pos;
} pb_cursor_t;

// Initialize cursor
static inline void pb_cursor_init(pb_cursor_t *c, const unsigned char *data, size_t length) {
    c->data = data;
    c->length = length;
    c->pos = 0;
}

// Decode varint
int pb_decode_varint(pb_cursor_t *c, uint64_t *value);

// Decode key (field number + wire type)
int pb_decode_key(pb_cursor_t *c, uint32_t *field, pb_wire_type_t *wire_type);

// Read raw bytes
int pb_read_bytes(pb_cursor_t *c, size_t count, const unsigned char **bytes);

// Decode length-delimited field
int pb_decode_length_delimited(pb_cursor_t *c, const unsigned char **data, size_t *length);

// Skip value
int pb_skip_value(pb_cursor_t *c, pb_wire_type_t wire_type);

#endif // PROTOBUF_WIRE_H
