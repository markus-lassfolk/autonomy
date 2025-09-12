#include "protobuf_wire.h"

int pb_decode_varint(pb_cursor_t *c, uint64_t *value) {
    if (!c || !value) return -1;
    
    *value = 0;
    int shift = 0;
    
    while (c->pos < c->length) {
        if (shift >= 64) return -1; // Overflow
        
        unsigned char byte = c->data[c->pos++];
        *value |= ((uint64_t)(byte & 0x7F)) << shift;
        
        if ((byte & 0x80) == 0) {
            return 0; // End of varint
        }
        
        shift += 7;
    }
    
    return -1; // Incomplete varint
}

int pb_decode_key(pb_cursor_t *c, uint32_t *field, pb_wire_type_t *wire_type) {
    if (!c || !field || !wire_type) return -1;
    
    uint64_t key;
    if (pb_decode_varint(c, &key) != 0) return -1;
    
    *field = (uint32_t)(key >> 3);
    *wire_type = (pb_wire_type_t)(key & 0x7);
    
    return 0;
}

int pb_read_bytes(pb_cursor_t *c, size_t count, const unsigned char **bytes) {
    if (!c || !bytes) return -1;
    
    if (c->pos + count > c->length) return -1;
    
    *bytes = c->data + c->pos;
    c->pos += count;
    
    return 0;
}

int pb_decode_length_delimited(pb_cursor_t *c, const unsigned char **data, size_t *length) {
    if (!c || !data || !length) return -1;
    
    uint64_t len;
    if (pb_decode_varint(c, &len) != 0) return -1;
    
    if (c->pos + len > c->length) return -1;
    
    *data = c->data + c->pos;
    *length = (size_t)len;
    c->pos += len;
    
    return 0;
}

int pb_skip_value(pb_cursor_t *c, pb_wire_type_t wire_type) {
    if (!c) return -1;
    
    switch (wire_type) {
        case PB_WIRE_VARINT: {
            uint64_t dummy;
            return pb_decode_varint(c, &dummy);
        }
        case PB_WIRE_64BIT: {
            const unsigned char *dummy;
            return pb_read_bytes(c, 8, &dummy);
        }
        case PB_WIRE_32BIT: {
            const unsigned char *dummy;
            return pb_read_bytes(c, 4, &dummy);
        }
        case PB_WIRE_LEN: {
            const unsigned char *dummy;
            size_t len;
            return pb_decode_length_delimited(c, &dummy, &len);
        }
        case PB_WIRE_START_GROUP:
        case PB_WIRE_END_GROUP:
            // Groups are deprecated, but we can skip them
            return 0;
        default:
            return -1;
    }
}







