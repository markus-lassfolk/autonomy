#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <curl/curl.h>

// Minimal includes from daemon
#include "protobuf_wire.h"

#define EMIT_KV(firstFlag, fmt, ...) do { \
    if (!(firstFlag)) printf(","); \
    printf(fmt, __VA_ARGS__); \
    (firstFlag) = 0; \
} while (0)

static void print_help(const char *prog) {
    printf("Usage: %s [host] [port] <command> [args]\n\n", prog);
    printf("Defaults: host=192.168.100.1, port=9200\n\n");
    printf("Commands:\n");
    printf("  get_device_info                -> prints getDeviceInfo JSON\n");
    printf("  get_status                     -> prints dishGetStatus JSON\n");
    printf("  get_history                    -> prints dishGetHistory JSON\n");
    printf("  get_location                   -> prints getLocation JSON\n");
    printf("  get_diagnostics                -> prints dishGetDiagnostics JSON\n");
    printf("  dish_get_obstruction_map       -> prints dishGetObstructionMap JSON\n");
    printf("  dish_clear_obstruction_map     -> prints dishClearObstructionMap JSON\n");
    printf("  dish_get_config                -> prints dishGetConfig JSON\n");
    printf("  dish_set_config \"k=v,...\"    -> sets DishConfig fields and prints result\n");
    printf("      keys: snowMeltMode(AUTO|ALWAYS_ON|ALWAYS_OFF), locationRequestMode(NONE|LOCAL),\n");
    printf("            levelDishMode(TILT_LIKE_NORMAL|FORCE_LEVEL), powerSaveStartMinutes(u32),\n");
    printf("            powerSaveDurationMinutes(u32), powerSaveMode(bool),\n");
    printf("            swupdateThreeDayDeferralEnabled(bool), assetClass(u32), swupdateRebootHour(u32),\n");
    printf("            apply* booleans e.g. applySnowMeltMode=true\n");
    printf("  reflect_dump [symbol] [out]    -> write protoset for symbol (default SpaceX.API.Device.Device to /tmp/dish.protoset)\n\n");
    printf("Examples:\n");
    printf("  %s --help\n", prog);
    printf("  %s 192.168.100.1 9200 get_status\n", prog);
    printf("  %s 192.168.100.1 9200 dish_set_config \"snowMeltMode=ALWAYS_OFF,applySnowMeltMode=true\"\n", prog);
    printf("  %s 192.168.100.1 9200 reflect_dump SpaceX.API.Device.Device /tmp/dish.protoset\n", prog);
}

typedef struct {
    unsigned char *data;
    size_t capacity;
    size_t length;
} curl_buffer_t;

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    curl_buffer_t *buf = (curl_buffer_t *)userdata;
    size_t n = size * nmemb;
    size_t left = (buf->capacity > buf->length) ? (buf->capacity - buf->length) : 0;
    size_t to_copy = n < left ? n : left;
    if (to_copy > 0) {
        memcpy(buf->data + buf->length, ptr, to_copy);
        buf->length += to_copy;
    }
    return n;
}

static size_t encode_varint(uint64_t value, unsigned char *out) {
    size_t i = 0;
    while (value >= 0x80) {
        out[i++] = (unsigned char)((value & 0x7F) | 0x80);
        value >>= 7;
    }
    out[i++] = (unsigned char)(value & 0x7F);
    return i;
}

static size_t encode_tag(uint32_t field_number, uint32_t wire_type, unsigned char *out) {
    uint64_t key = (((uint64_t)field_number) << 3) | (uint64_t)wire_type;
    return encode_varint(key, out);
}

static int encode_length_delimited_field(uint32_t field_number, const unsigned char *data, size_t data_len, unsigned char *out, size_t *out_len, size_t cap) {
    size_t pos = 0;
    pos += encode_tag(field_number, 2, out + pos);
    pos += encode_varint((uint64_t)data_len, out + pos);
    if (pos + data_len > cap) return -1;
    memcpy(out + pos, data, data_len);
    pos += data_len;
    *out_len = pos;
    return 0;
}

static int build_request_oneof(uint32_t field_number, unsigned char *out, size_t *out_len, size_t cap) {
    if (cap < 2) return -1;
    uint64_t key = (((uint64_t)field_number) << 3) | 2ULL; // len-delimited
    size_t pos = 0;
    pos += encode_varint(key, out + pos);
    out[pos++] = 0x00; // zero-length embedded message
    *out_len = pos;
    return 0;
}

// Build DishConfig message from simple kv string: key=value[,key=value...]
// Supported keys (snake or camel):
// snow_melt_mode(AUTO|ALWAYS_ON|ALWAYS_OFF), location_request_mode(NONE|LOCAL), level_dish_mode(TILT_LIKE_NORMAL|FORCE_LEVEL)
// power_save_start_minutes(u32), power_save_duration_minutes(u32), power_save_mode(bool), swupdate_three_day_deferral_enabled(bool)
// asset_class(u32), swupdate_reboot_hour(u32)
// apply_* booleans for each above (e.g., apply_snow_melt_mode=true)
static int build_dish_config_message(const char *kv, unsigned char *out, size_t *out_len, size_t cap) {
    // Use a simple append of individual fields into out
    size_t pos = 0;
    if (!kv || kv[0] == '\0') { *out_len = 0; return 0; }

    // Copy kv to mutable buffer
    char *tmp = strdup(kv);
    if (!tmp) return -1;
    for (char *p = tmp; *p; ++p) if (*p == '\n') *p = ',';
    char *save = NULL;
    char *tok = strtok_r(tmp, ",", &save);
    while (tok) {
        while (*tok == ' ' || *tok == '\t') tok++;
        char *eq = strchr(tok, '=');
        if (eq) {
            *eq = '\0';
            const char *key = tok;
            const char *val = eq + 1;
            // normalize key to snake
            // snow_melt_mode
            if (strcasecmp(key, "snow_melt_mode") == 0 || strcasecmp(key, "snowMeltMode") == 0) {
                uint64_t e = 0; // AUTO
                if (strcasecmp(val, "ALWAYS_ON") == 0) e = 1; else if (strcasecmp(val, "ALWAYS_OFF") == 0) e = 2; else e = 0;
                unsigned char buf[16]; size_t n = 0; n += encode_tag(1, 0, buf + n); n += encode_varint(e, buf + n);
                if (pos + n > cap) { free(tmp); return -1; } memcpy(out + pos, buf, n); pos += n;
            } else if (strcasecmp(key, "location_request_mode") == 0 || strcasecmp(key, "locationRequestMode") == 0) {
                uint64_t e = (strcasecmp(val, "LOCAL") == 0) ? 1 : 0;
                unsigned char buf[16]; size_t n = 0; n += encode_tag(2, 0, buf + n); n += encode_varint(e, buf + n);
                if (pos + n > cap) { free(tmp); return -1; } memcpy(out + pos, buf, n); pos += n;
            } else if (strcasecmp(key, "level_dish_mode") == 0 || strcasecmp(key, "levelDishMode") == 0) {
                uint64_t e = (strcasecmp(val, "FORCE_LEVEL") == 0) ? 1 : 0;
                unsigned char buf[16]; size_t n = 0; n += encode_tag(3, 0, buf + n); n += encode_varint(e, buf + n);
                if (pos + n > cap) { free(tmp); return -1; } memcpy(out + pos, buf, n); pos += n;
            } else if (strcasecmp(key, "power_save_start_minutes") == 0 || strcasecmp(key, "powerSaveStartMinutes") == 0) {
                uint64_t v = strtoull(val, NULL, 10);
                unsigned char buf[16]; size_t n = 0; n += encode_tag(4, 0, buf + n); n += encode_varint(v, buf + n);
                if (pos + n > cap) { free(tmp); return -1; } memcpy(out + pos, buf, n); pos += n;
            } else if (strcasecmp(key, "power_save_duration_minutes") == 0 || strcasecmp(key, "powerSaveDurationMinutes") == 0) {
                uint64_t v = strtoull(val, NULL, 10);
                unsigned char buf[16]; size_t n = 0; n += encode_tag(5, 0, buf + n); n += encode_varint(v, buf + n);
                if (pos + n > cap) { free(tmp); return -1; } memcpy(out + pos, buf, n); pos += n;
            } else if (strcasecmp(key, "power_save_mode") == 0 || strcasecmp(key, "powerSaveMode") == 0) {
                uint64_t v = (strcasecmp(val, "true") == 0 || strcmp(val, "1") == 0) ? 1 : 0;
                unsigned char buf[16]; size_t n = 0; n += encode_tag(6, 0, buf + n); n += encode_varint(v, buf + n);
                if (pos + n > cap) { free(tmp); return -1; } memcpy(out + pos, buf, n); pos += n;
            } else if (strcasecmp(key, "swupdate_three_day_deferral_enabled") == 0 || strcasecmp(key, "swupdateThreeDayDeferralEnabled") == 0) {
                uint64_t v = (strcasecmp(val, "true") == 0 || strcmp(val, "1") == 0) ? 1 : 0;
                unsigned char buf[16]; size_t n = 0; n += encode_tag(7, 0, buf + n); n += encode_varint(v, buf + n);
                if (pos + n > cap) { free(tmp); return -1; } memcpy(out + pos, buf, n); pos += n;
            } else if (strcasecmp(key, "asset_class") == 0 || strcasecmp(key, "assetClass") == 0) {
                uint64_t v = strtoull(val, NULL, 10);
                unsigned char buf[16]; size_t n = 0; n += encode_tag(8, 0, buf + n); n += encode_varint(v, buf + n);
                if (pos + n > cap) { free(tmp); return -1; } memcpy(out + pos, buf, n); pos += n;
            } else if (strcasecmp(key, "swupdate_reboot_hour") == 0 || strcasecmp(key, "swupdateRebootHour") == 0) {
                uint64_t v = strtoull(val, NULL, 10);
                unsigned char buf[16]; size_t n = 0; n += encode_tag(9, 0, buf + n); n += encode_varint(v, buf + n);
                if (pos + n > cap) { free(tmp); return -1; } memcpy(out + pos, buf, n); pos += n;
            } else if (strncasecmp(key, "apply_", 6) == 0 || strncasecmp(key, "apply", 5) == 0) {
                uint32_t fnum = 0;
                if (strcasecmp(key, "apply_snow_melt_mode") == 0 || strcasecmp(key, "applySnowMeltMode") == 0) fnum = 1001;
                else if (strcasecmp(key, "apply_location_request_mode") == 0 || strcasecmp(key, "applyLocationRequestMode") == 0) fnum = 2001;
                else if (strcasecmp(key, "apply_level_dish_mode") == 0 || strcasecmp(key, "applyLevelDishMode") == 0) fnum = 3001;
                else if (strcasecmp(key, "apply_power_save_start_minutes") == 0 || strcasecmp(key, "applyPowerSaveStartMinutes") == 0) fnum = 4001;
                else if (strcasecmp(key, "apply_power_save_duration_minutes") == 0 || strcasecmp(key, "applyPowerSaveDurationMinutes") == 0) fnum = 5001;
                else if (strcasecmp(key, "apply_power_save_mode") == 0 || strcasecmp(key, "applyPowerSaveMode") == 0) fnum = 6001;
                else if (strcasecmp(key, "apply_swupdate_three_day_deferral_enabled") == 0 || strcasecmp(key, "applySwupdateThreeDayDeferralEnabled") == 0) fnum = 7001;
                else if (strcasecmp(key, "apply_asset_class") == 0 || strcasecmp(key, "applyAssetClass") == 0) fnum = 8001;
                else if (strcasecmp(key, "apply_swupdate_reboot_hour") == 0 || strcasecmp(key, "applySwupdateRebootHour") == 0) fnum = 9001;
                if (fnum != 0) {
                    uint64_t v = (strcasecmp(val, "true") == 0 || strcmp(val, "1") == 0) ? 1 : 0;
                    unsigned char buf[16]; size_t n = 0; n += encode_tag(fnum, 0, buf + n); n += encode_varint(v, buf + n);
                    if (pos + n > cap) { free(tmp); return -1; } memcpy(out + pos, buf, n); pos += n;
                }
            }
        }
        tok = strtok_r(NULL, ",", &save);
    }
    free(tmp);
    *out_len = pos;
    return 0;
}

static int frame_grpc(const unsigned char *msg, size_t msg_len, unsigned char *out, size_t *out_len, size_t cap) {
    if (cap < msg_len + 5) return -1;
    out[0] = 0; // no compression
    out[1] = (unsigned char)((msg_len >> 24) & 0xFF);
    out[2] = (unsigned char)((msg_len >> 16) & 0xFF);
    out[3] = (unsigned char)((msg_len >> 8) & 0xFF);
    out[4] = (unsigned char)(msg_len & 0xFF);
    memcpy(out + 5, msg, msg_len);
    *out_len = msg_len + 5;
    return 0;
}

static int extract_first_message(const unsigned char *buf, size_t len, const unsigned char **msg, size_t *msg_len) {
    size_t pos = 0;
    while (pos + 5 <= len) {
        unsigned char compressed = buf[pos++];
        uint32_t mlen = ((uint32_t)buf[pos] << 24) | ((uint32_t)buf[pos+1] << 16) | ((uint32_t)buf[pos+2] << 8) | (uint32_t)buf[pos+3];
        pos += 4;
        if (pos + mlen > len) return -1;
        if (compressed == 0 && mlen > 0) {
            *msg = &buf[pos];
            *msg_len = mlen;
            return 0;
        }
        pos += mlen;
    }
    return -1;
}

// Minimal reflection: request file descriptors for a symbol and write a FileDescriptorSet protoset
static int reflection_dump_protoset(const char *host, int port, const char *symbol, const char *out_path) {
    // Build ServerReflectionRequest{ file_containing_symbol: symbol }
    unsigned char symbuf[512]; size_t symlen = strlen(symbol);
    if (symlen > sizeof(symbuf)) return -1;
    memcpy(symbuf, symbol, symlen);

    unsigned char reqmsg[600]; size_t reqmsg_len = 0;
    if (encode_length_delimited_field(4, symbuf, symlen, reqmsg, &reqmsg_len, sizeof reqmsg) != 0) return -1;

    unsigned char frame[700]; size_t frame_len = 0;
    if (frame_grpc(reqmsg, reqmsg_len, frame, &frame_len, sizeof frame) != 0) return -1;

    char url[256]; snprintf(url, sizeof url, "http://%s:%d/grpc.reflection.v1alpha.ServerReflection/ServerReflectionInfo", host, port);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init(); if (!curl) return -1;
    struct curl_slist *hdr = NULL;
    hdr = curl_slist_append(hdr, "Content-Type: application/grpc");
    hdr = curl_slist_append(hdr, "TE: trailers");
    hdr = curl_slist_append(hdr, "User-Agent: starlink-standalone/1.0");

    unsigned char resp[256 * 1024]; memset(resp, 0, sizeof resp);
    curl_buffer_t buf = { .data = resp, .capacity = sizeof resp, .length = 0 };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, frame);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)frame_len);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode rc = curl_easy_perform(curl);
    curl_slist_free_all(hdr);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    if (rc != CURLE_OK) return -1;

    // Iterate all frames; collect file_descriptor_proto bytes
    size_t pos = 0; unsigned char fdbufs[256 * 1024]; size_t fdbufs_len = 0;
    while (pos + 5 <= buf.length) {
        unsigned char compressed = resp[pos++];
        uint32_t mlen = ((uint32_t)resp[pos] << 24) | ((uint32_t)resp[pos+1] << 16) | ((uint32_t)resp[pos+2] << 8) | (uint32_t)resp[pos+3];
        pos += 4;
        if (pos + mlen > buf.length) break;
        if (compressed == 0 && mlen > 0) {
            const unsigned char *m = &resp[pos]; size_t ml = mlen;
            pb_cursor_t c; pb_cursor_init(&c, m, ml);
            uint32_t f; pb_wire_type_t wt;
            while (c.pos < c.length) {
                if (pb_decode_key(&c, &f, &wt) != 0) break;
                if (f == 4 && wt == PB_WIRE_LEN) { // file_descriptor_response
                    const unsigned char *ld; size_t ldlen; if (pb_decode_length_delimited(&c,&ld,&ldlen)!=0) break;
                    pb_cursor_t d; pb_cursor_init(&d, ld, ldlen);
                    while (d.pos < d.length) {
                        uint32_t df; pb_wire_type_t dw; if (pb_decode_key(&d,&df,&dw)!=0) break;
                        if (df == 1 && dw == PB_WIRE_LEN) { const unsigned char *fd; size_t fdlen; if (pb_decode_length_delimited(&d,&fd,&fdlen)!=0) break;
                            // Append as FileDescriptorSet.file field (1)
                            unsigned char hdrb[16]; size_t hn = 0; hn += encode_tag(1, 2, hdrb + hn); hn += encode_varint(fdlen, hdrb + hn);
                            if (fdbufs_len + hn + fdlen > sizeof fdbufs) break;
                            memcpy(fdbufs + fdbufs_len, hdrb, hn); fdbufs_len += hn;
                            memcpy(fdbufs + fdbufs_len, fd, fdlen); fdbufs_len += fdlen;
                        } else { if (pb_skip_value(&d,dw)!=0) break; }
                    }
                } else { if (pb_skip_value(&c, wt)!=0) break; }
            }
        }
        pos += mlen;
    }
    if (fdbufs_len == 0) return -1;

    FILE *fp = fopen(out_path, "wb"); if (!fp) return -1;
    // Write FileDescriptorSet message: it's already a concatenation of field-1 entries
    fwrite(fdbufs, 1, fdbufs_len, fp);
    fclose(fp);
    return 0;
}

// Use reflection to get FileDescriptorProto for SpaceX.API.Device.Request and list oneof 'request' field names
static int list_available_calls(const char *host, int port) {
    const char *symbol = "SpaceX.API.Device.Request";
    unsigned char symbuf[256]; size_t symlen = strlen(symbol);
    if (symlen >= sizeof symbuf) return -1;
    memcpy(symbuf, symbol, symlen);

    unsigned char reqmsg[600]; size_t reqmsg_len = 0;
    if (encode_length_delimited_field(4, symbuf, symlen, reqmsg, &reqmsg_len, sizeof reqmsg) != 0) return -1;
    unsigned char frame[700]; size_t frame_len = 0;
    if (frame_grpc(reqmsg, reqmsg_len, frame, &frame_len, sizeof frame) != 0) return -1;

    char url[256]; snprintf(url, sizeof url, "http://%s:%d/grpc.reflection.v1alpha.ServerReflection/ServerReflectionInfo", host, port);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init(); if (!curl) return -1;
    struct curl_slist *hdr = NULL;
    hdr = curl_slist_append(hdr, "Content-Type: application/grpc");
    hdr = curl_slist_append(hdr, "TE: trailers");
    hdr = curl_slist_append(hdr, "User-Agent: starlink-standalone/1.0");
    unsigned char resp[256 * 1024]; memset(resp, 0, sizeof resp);
    curl_buffer_t buf = { .data = resp, .capacity = sizeof resp, .length = 0 };
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, frame);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)frame_len);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    CURLcode rc = curl_easy_perform(curl);
    curl_slist_free_all(hdr);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    if (rc != CURLE_OK) return -1;

    // Parse frames to find file_descriptor_response.file_descriptor_proto bytes
    size_t pos = 0;
    int listed = 0;
    while (pos + 5 <= buf.length) {
        unsigned char compressed = resp[pos++];
        uint32_t mlen = ((uint32_t)resp[pos] << 24) | ((uint32_t)resp[pos+1] << 16) | ((uint32_t)resp[pos+2] << 8) | (uint32_t)resp[pos+3];
        pos += 4;
        if (pos + mlen > buf.length) break;
        if (compressed == 0 && mlen > 0) {
            const unsigned char *m = &resp[pos]; size_t ml = mlen;
            pb_cursor_t c; pb_cursor_init(&c, m, ml);
            uint32_t f; pb_wire_type_t wt;
            while (c.pos < c.length) {
                if (pb_decode_key(&c, &f, &wt) != 0) break;
                if (f == 4 && wt == PB_WIRE_LEN) { // file_descriptor_response
                    const unsigned char *ld; size_t ldlen; if (pb_decode_length_delimited(&c,&ld,&ldlen)!=0) break;
                    pb_cursor_t d; pb_cursor_init(&d, ld, ldlen);
                    while (d.pos < d.length) {
                        uint32_t df; pb_wire_type_t dw; if (pb_decode_key(&d,&df,&dw)!=0) break;
                        if (df == 1 && dw == PB_WIRE_LEN) { // bytes file_descriptor_proto
                            const unsigned char *fd; size_t fdlen; if (pb_decode_length_delimited(&d,&fd,&fdlen)!=0) break;
                            // Parse FileDescriptorProto -> message_type (4) -> DescriptorProto with name "Request"
                            pb_cursor_t fdcur; pb_cursor_init(&fdcur, fd, fdlen);
                            while (fdcur.pos < fdcur.length) {
                                uint32_t ff; pb_wire_type_t fw; if (pb_decode_key(&fdcur,&ff,&fw)!=0) break;
                                if (ff == 4 && fw == PB_WIRE_LEN) { // message_type
                                    const unsigned char *mt; size_t mtlen; if (pb_decode_length_delimited(&fdcur,&mt,&mtlen)!=0) break;
                                    pb_cursor_t mtc; pb_cursor_init(&mtc, mt, mtlen);
                                    // DescriptorProto loop
                                    while (mtc.pos < mtc.length) {
                                        const unsigned char *dp; size_t dplen; // each DescriptorProto is length-delimited
                                        uint32_t mf; pb_wire_type_t mw; if (pb_decode_key(&mtc,&mf,&mw)!=0) break;
                                        if (mf == 0) break; // invalid
                                        if (mw != PB_WIRE_LEN) { if (pb_skip_value(&mtc,mw)!=0) break; continue; }
                                        if (pb_decode_length_delimited(&mtc, &dp, &dplen) != 0) break;
                                        pb_cursor_t dpc; pb_cursor_init(&dpc, dp, dplen);
                                        // Extract name (1)
                                        char msgname[128]={0}; int have_name=0;
                                        // Store oneof_decl names to find index of "request"
                                        int request_oneof_index = -1; int current_oneof_index = 0;
                                        // First pass: find name and oneof_decl indices
                                        size_t save_pos = dpc.pos;
                                        while (dpc.pos < dpc.length) {
                                            uint32_t df2; pb_wire_type_t dw2; if (pb_decode_key(&dpc,&df2,&dw2)!=0) break;
                                            if (df2 == 1 && dw2 == PB_WIRE_LEN) { const unsigned char *s; size_t sl; if(pb_decode_length_delimited(&dpc,&s,&sl)==0){ size_t n=sl<sizeof(msgname)-1?sl:sizeof(msgname)-1; memcpy(msgname,s,n); msgname[n]='\0'; have_name=1; } }
                                            else if (df2 == 8 && dw2 == PB_WIRE_LEN) { // oneof_decl repeated
                                                const unsigned char *oo; size_t oolen; if (pb_decode_length_delimited(&dpc,&oo,&oolen)!=0) break; pb_cursor_t ooc; pb_cursor_init(&ooc,oo,oolen);
                                                while (ooc.pos < ooc.length) {
                                                    uint32_t of; pb_wire_type_t ow; if (pb_decode_key(&ooc,&of,&ow)!=0) break;
                                                    if (of == 1 && ow == PB_WIRE_LEN) { const unsigned char *s; size_t sl; if(pb_decode_length_delimited(&ooc,&s,&sl)==0){ if (sl==7 && memcmp(s, "request", 7) == 0) request_oneof_index = current_oneof_index; } }
                                                    else { if (pb_skip_value(&ooc,ow)!=0) break; }
                                                }
                                                current_oneof_index++;
                                            } else { if (pb_skip_value(&dpc,dw2)!=0) break; }
                                        }
                                        if (!(have_name && strcmp(msgname, "Request") == 0) || request_oneof_index < 0) {
                                            continue;
                                        }
                                        // Second pass: iterate fields and collect those with oneof_index == request_oneof_index
                                        dpc.pos = save_pos;
                                        printf("Available Request calls (oneof 'request'):\n");
                                        while (dpc.pos < dpc.length) {
                                            uint32_t df2; pb_wire_type_t dw2; if (pb_decode_key(&dpc,&df2,&dw2)!=0) break;
                                            if (df2 == 2 && dw2 == PB_WIRE_LEN) { // FieldDescriptorProto
                                                const unsigned char *fdp; size_t fdplen; if (pb_decode_length_delimited(&dpc,&fdp,&fdplen)!=0) break; pb_cursor_t fdc; pb_cursor_init(&fdc,fdp,fdplen);
                                                char fname[128]={0}; int have_fname=0; int32_t oneof_idx=-1;
                                                while (fdc.pos < fdc.length) {
                                                    uint32_t ff2; pb_wire_type_t fw2; if (pb_decode_key(&fdc,&ff2,&fw2)!=0) break;
                                                    if (ff2 == 1 && fw2 == PB_WIRE_LEN) { const unsigned char *s; size_t sl; if(pb_decode_length_delimited(&fdc,&s,&sl)==0){ size_t n=sl<sizeof(fname)-1?sl:sizeof(fname)-1; memcpy(fname,s,n); fname[n]='\0'; have_fname=1; } }
                                                    else if (ff2 == 9 && fw2 == PB_WIRE_VARINT) { uint64_t v; if(pb_decode_varint(&fdc,&v)==0) oneof_idx = (int32_t)v; }
                                                    else { if (pb_skip_value(&fdc,fw2)!=0) break; }
                                                }
                                                if (have_fname && oneof_idx == request_oneof_index) {
                                                    printf("- %s\n", fname);
                                                    listed = 1;
                                                }
                                            } else { if (pb_skip_value(&dpc,dw2)!=0) break; }
                                        }
                                    }
                                } else { if (pb_skip_value(&fdcur,fw)!=0) break; }
                            }
                        } else { if (pb_skip_value(&d,dw)!=0) break; }
                    }
                } else { if (pb_skip_value(&c, wt)!=0) break; }
            }
        }
        pos += mlen;
    }
    if (!listed) {
        // Fallback: print a known set from observed firmware
        printf("Available Request calls (fallback list):\n");
        const char *names[] = {
            "reboot","speed_test","get_status","authenticate","get_next_id","get_history","get_device_info","get_ping","set_trusted_keys","factory_reset","get_log","set_sku","update","get_network_interfaces","ping_host","get_location","get_heap_dump","restart_control","fuse","get_persistent_stats","get_connections","start_speedtest","get_speedtest_status","report_client_speedtest","self_test","set_test_mode","software_update","enable_debug_telem","iq_capture","get_radio_stats","time","run_iperf_server","tcp_connectivity_test","udp_connectivity_test","get_goroutine_stack_traces","dish_stow","dish_get_context","dish_set_emc","dish_get_obstruction_map","dish_get_emc","dish_set_config","dish_get_config","dish_power_save","dish_inhibit_gps","dish_get_data","dish_clear_obstruction_map","dish_set_max_power_test_mode","dish_activate_rssi_scan","dish_get_rssi_scan_result","dish_factory_reset","reset_button","set_per_vehicle_config","dish_aviation_test","wifi_set_config","wifi_get_clients","wifi_setup","wifi_get_ping_metrics","wifi_get_config","wifi_set_mesh_device_trust","wifi_set_mesh_config","wifi_get_client_history","wifi_set_aviation_conformed","wifi_set_client_given_name","wifi_self_test","wifi_calibration_mode","wifi_guest_info","wifi_rf_test","wifi_get_firewall","wifi_toggle_poe_negotiation","wifi_factory_test_command","wifi_start_local_telem_proxy","wifi_run_self_test","wifi_backhaul_stats","wifi_toggle_umbilical_mode","wifi_client_sandbox","transceiver_if_loopback_test","transceiver_get_status","transceiver_get_telemetry","start_unlock","finish_unlock","get_diagnostics"
        };
        size_t n = sizeof(names)/sizeof(names[0]);
        for (size_t i=0;i<n;i++) printf("- %s\n", names[i]);
        return 0;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *host = argc > 1 ? argv[1] : "192.168.100.1";
    const int port = argc > 2 ? atoi(argv[2]) : 9200;
    const char *method = argc > 3 ? argv[3] : "get_device_info"; // see --help for list

    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_help(argv[0]);
        return 0;
    }
    if (argc > 3 && (strcmp(method, "--help") == 0 || strcmp(method, "-h") == 0)) {
        print_help(argv[0]);
        return 0;
    }

    if (strcmp(method, "help") == 0) {
        print_help(argv[0]);
        return 0;
    }
    if (strcmp(method, "list") == 0) {
        if (list_available_calls(host, port) != 0) return 8;
        return 0;
    }
    if (strcmp(method, "reflect_dump") == 0) {
        const char *symbol = argc > 4 ? argv[4] : "SpaceX.API.Device.Device";
        const char *outp = argc > 5 ? argv[5] : "/tmp/dish.protoset";
        int rr = reflection_dump_protoset(host, port, symbol, outp);
        if (rr == 0) { printf("wrote protoset: %s\n", outp); return 0; }
        fprintf(stderr, "reflection dump failed\n");
        return 7;
    }

    uint32_t field = 0;
    if (strcmp(method, "get_device_info") == 0) field = 1008;
    else if (strcmp(method, "get_status") == 0) field = 1004;
    else if (strcmp(method, "get_history") == 0) field = 1007;
    else if (strcmp(method, "get_location") == 0) field = 1017;
    else if (strcmp(method, "get_diagnostics") == 0) field = 6000;
    else if (strcmp(method, "dish_get_config") == 0) field = 2011;
    else if (strcmp(method, "dish_set_config") == 0) field = 2010;
    else if (strcmp(method, "dish_get_obstruction_map") == 0) field = 2008;
    else if (strcmp(method, "dish_clear_obstruction_map") == 0) field = 2017;
    else {
        fprintf(stderr, "Unknown method: %s\n", method);
        return 2;
    }

    unsigned char req[128]; size_t req_len = 0;
    if (strcmp(method, "dish_set_config") == 0) {
        // Build nested DishSetConfigRequest{dish_config: DishConfig{...}}
        const char *cfg = argc > 4 ? argv[4] : "";
        unsigned char dishcfg[512]; size_t dishcfg_len = 0;
        if (build_dish_config_message(cfg, dishcfg, &dishcfg_len, sizeof dishcfg) != 0) {
            fprintf(stderr, "Failed to build DishConfig from args\n");
            return 3;
        }
        unsigned char inner[600]; size_t inner_len = 0; // DishSetConfigRequest
        if (encode_length_delimited_field(1, dishcfg, dishcfg_len, inner, &inner_len, sizeof inner) != 0) {
            fprintf(stderr, "Failed to build DishSetConfigRequest\n");
            return 3;
        }
        // Wrap in Request oneof field 2010
        size_t pos = 0;
        pos += encode_tag(2010, 2, req + pos);
        pos += encode_varint(inner_len, req + pos);
        if (pos + inner_len > sizeof req) { fprintf(stderr, "Buffer too small\n"); return 3; }
        memcpy(req + pos, inner, inner_len);
        pos += inner_len;
        req_len = pos;
    } else {
        if (build_request_oneof(field, req, &req_len, sizeof req) != 0) {
            fprintf(stderr, "Failed to build request\n");
            return 3;
        }
    }

    unsigned char frame[256]; size_t frame_len = 0;
    if (frame_grpc(req, req_len, frame, &frame_len, sizeof frame) != 0) {
        fprintf(stderr, "Failed to frame gRPC message\n");
        return 4;
    }

    char url[256];
    snprintf(url, sizeof url, "http://%s:%d/SpaceX.API.Device.Device/Handle", host, port);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();
    if (!curl) { fprintf(stderr, "curl init failed\n"); return 5; }

    struct curl_slist *hdr = NULL;
    hdr = curl_slist_append(hdr, "Content-Type: application/grpc");
    hdr = curl_slist_append(hdr, "grpc-encoding: identity");
    hdr = curl_slist_append(hdr, "grpc-accept-encoding: identity");
    hdr = curl_slist_append(hdr, "TE: trailers");
    hdr = curl_slist_append(hdr, "User-Agent: starlink-standalone/1.0");

    unsigned char resp[64 * 1024]; memset(resp, 0, sizeof resp);
    curl_buffer_t buf = { .data = resp, .capacity = sizeof resp, .length = 0 };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, frame);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)frame_len);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode rc = curl_easy_perform(curl);
    long http = 0; curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    if (rc != CURLE_OK) {
        fprintf(stderr, "curl error: %s (http %ld)\n", curl_easy_strerror(rc), http);
        curl_slist_free_all(hdr);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 6;
    }
    printf("HTTP %ld, bytes %zu\n", http, buf.length);

    const unsigned char *msg; size_t msg_len;
    if (extract_first_message(resp, buf.length, &msg, &msg_len) == 0) {
        // Two-pass decode: first collect status/apiVersion, then parse method payload
        pb_cursor_t pre; pb_cursor_init(&pre, msg, msg_len);
        uint64_t api_version = 0; int have_api_version = 0;
        while (pre.pos < pre.length) {
            uint32_t f; pb_wire_type_t wt;
            if (pb_decode_key(&pre, &f, &wt) != 0) break;
            if (f == 2 && wt == PB_WIRE_LEN) { const unsigned char *ld; size_t ldlen; if (pb_decode_length_delimited(&pre,&ld,&ldlen)!=0) break; }
            else if (f == 3 && wt == PB_WIRE_VARINT) { uint64_t v; if (pb_decode_varint(&pre,&v)!=0) break; api_version = v; have_api_version = 1; }
            else { if (pb_skip_value(&pre, wt) != 0) break; }
        }

        pb_cursor_t c; pb_cursor_init(&c, msg, msg_len);
        int handled = 0; uint32_t f; pb_wire_type_t wt;
        while (c.pos < c.length) {
            if (pb_decode_key(&c, &f, &wt) != 0) break;
            if (f == 2 && wt == PB_WIRE_LEN) { const unsigned char *ld; size_t ldlen; if (pb_decode_length_delimited(&c,&ld,&ldlen)!=0) break; continue; }
            if (f == 3 && wt == PB_WIRE_VARINT) { uint64_t v; if (pb_decode_varint(&c,&v)!=0) break; continue; }
            if (wt == PB_WIRE_LEN) {
                const unsigned char *ld; size_t ldlen; if (pb_decode_length_delimited(&c,&ld,&ldlen) != 0) break;
                // Dispatch based on requested method
                if (strcmp(method, "get_device_info") == 0) {
                    pb_cursor_t d; pb_cursor_init(&d, ld, ldlen);
                    char id[128] = {0}, hw[64] = {0}, swv[64] = {0}, cc[8] = {0};
                    while (d.pos < d.length) {
                        uint32_t df; pb_wire_type_t dw; if (pb_decode_key(&d,&df,&dw)!=0) break;
                        if (df == 1 && dw == PB_WIRE_LEN) {
                            const unsigned char *di; size_t dilen; if (pb_decode_length_delimited(&d,&di,&dilen)!=0) break; pb_cursor_t e; pb_cursor_init(&e, di, dilen);
                            while (e.pos < e.length) {
                                uint32_t ef; pb_wire_type_t ew; if (pb_decode_key(&e,&ef,&ew)!=0) break;
                                if (ef == 1 && ew == PB_WIRE_LEN) { const unsigned char *s; size_t sl; if(pb_decode_length_delimited(&e,&s,&sl)==0){ size_t n=sl<sizeof(id)-1?sl:sizeof(id)-1; memcpy(id,s,n); id[n]='\0'; } }
                                else if (ef == 2 && ew == PB_WIRE_LEN) { const unsigned char *s; size_t sl; if(pb_decode_length_delimited(&e,&s,&sl)==0){ size_t n=sl<sizeof(hw)-1?sl:sizeof(hw)-1; memcpy(hw,s,n); hw[n]='\0'; } }
                                else if (ef == 3 && ew == PB_WIRE_LEN) { const unsigned char *s; size_t sl; if(pb_decode_length_delimited(&e,&s,&sl)==0){ size_t n=sl<sizeof(swv)-1?sl:sizeof(swv)-1; memcpy(swv,s,n); swv[n]='\0'; } }
                                else if (ef == 4 && ew == PB_WIRE_LEN) { const unsigned char *s; size_t sl; if(pb_decode_length_delimited(&e,&s,&sl)==0){ size_t n=sl<sizeof(cc)-1?sl:sizeof(cc)-1; memcpy(cc,s,n); cc[n]='\0'; } }
                                else { if (pb_skip_value(&e, ew) != 0) break; }
                            }
                        } else { if (pb_skip_value(&d,dw)!=0) break; }
                    }
                    if (have_api_version)
                        printf("{\"apiVersion\":\"%llu\",\"getDeviceInfo\":{\"deviceInfo\":{\"id\":\"%s\",\"hardwareVersion\":\"%s\",\"softwareVersion\":\"%s\",\"countryCode\":\"%s\"}}}\n", (unsigned long long)api_version, id, hw, swv, cc);
                    else
                        printf("{\"getDeviceInfo\":{\"deviceInfo\":{\"id\":\"%s\",\"hardwareVersion\":\"%s\",\"softwareVersion\":\"%s\",\"countryCode\":\"%s\"}}}\n", id, hw, swv, cc);
                    handled = 1; break;
                }
                if (strcmp(method, "get_status") == 0) {
                    pb_cursor_t d; pb_cursor_init(&d, ld, ldlen);
                    double pop_latency=0, drop_rate=0, dl=0, ul=0, obstruct=0, bore_az=0, bore_el=0, uptime=0; int gps_sats=0; int gps_valid=0;
                    while (d.pos < d.length) {
                        uint32_t df; pb_wire_type_t dw; if (pb_decode_key(&d,&df,&dw)!=0) break;
                        if (df == 2 && dw == PB_WIRE_LEN) { const unsigned char *ds; size_t dslen; if(pb_decode_length_delimited(&d,&ds,&dslen)!=0) break; pb_cursor_t e; pb_cursor_init(&e,ds,dslen); while(e.pos<e.length){ uint32_t ff; pb_wire_type_t fw; if(pb_decode_key(&e,&ff,&fw)!=0) break; if(ff==1 && fw==PB_WIRE_VARINT){ uint64_t v; if(pb_decode_varint(&e,&v)==0) uptime=(double)v; } else { if(pb_skip_value(&e,fw)!=0) break; } } }
                        else if (df == 1009 && dw == PB_WIRE_32BIT) { const unsigned char *p; if(pb_read_bytes(&d,4,&p)==0){ float fv; memcpy(&fv,p,4); pop_latency=fv; } }
                        else if (df == 1003 && dw == PB_WIRE_32BIT) { const unsigned char *p; if(pb_read_bytes(&d,4,&p)==0){ float fv; memcpy(&fv,p,4); drop_rate=fv; } }
                        else if (df == 1007 && dw == PB_WIRE_32BIT) { const unsigned char *p; if(pb_read_bytes(&d,4,&p)==0){ float fv; memcpy(&fv,p,4); dl=fv; } }
                        else if (df == 1008 && dw == PB_WIRE_32BIT) { const unsigned char *p; if(pb_read_bytes(&d,4,&p)==0){ float fv; memcpy(&fv,p,4); ul=fv; } }
                        else if (df == 1004 && dw == PB_WIRE_LEN) { const unsigned char *os; size_t oslen; if(pb_decode_length_delimited(&d,&os,&oslen)==0){ pb_cursor_t e; pb_cursor_init(&e,os,oslen); while(e.pos<e.length){ uint32_t ff; pb_wire_type_t fw; if(pb_decode_key(&e,&ff,&fw)!=0) break; if(ff==1 && fw==PB_WIRE_32BIT){ const unsigned char *p; if(pb_read_bytes(&e,4,&p)==0){ float fv; memcpy(&fv,p,4); obstruct=fv; } } else { if(pb_skip_value(&e,fw)!=0) break; } } } }
                        else if (df == 1015 && dw == PB_WIRE_LEN) { const unsigned char *gs; size_t gslen; if(pb_decode_length_delimited(&d,&gs,&gslen)==0){ pb_cursor_t e; pb_cursor_init(&e,gs,gslen); while(e.pos<e.length){ uint32_t ff; pb_wire_type_t fw; if(pb_decode_key(&e,&ff,&fw)!=0) break; if(ff==1 && fw==PB_WIRE_VARINT){ uint64_t v; if(pb_decode_varint(&e,&v)==0) gps_valid=(v!=0); } else if (ff==2 && fw==PB_WIRE_VARINT){ uint64_t v; if(pb_decode_varint(&e,&v)==0) gps_sats=(int)v; } else { if(pb_skip_value(&e,fw)!=0) break; } } } }
                        else if (df == 1011 && dw == PB_WIRE_32BIT) { const unsigned char *p; if(pb_read_bytes(&d,4,&p)==0){ float fv; memcpy(&fv,p,4); bore_az=fv; } }
                        else if (df == 1012 && dw == PB_WIRE_32BIT) { const unsigned char *p; if(pb_read_bytes(&d,4,&p)==0){ float fv; memcpy(&fv,p,4); bore_el=fv; } }
                        else { if (pb_skip_value(&d,dw)!=0) break; }
                    }
                    if (have_api_version)
                        printf("{\"apiVersion\":\"%llu\",\"dishGetStatus\":{\"deviceState\":{\"uptimeS\":%.0f},\"popPingLatencyMs\":%.2f,\"popPingDropRate\":%.3f,\"downlinkThroughputBps\":%.2f,\"uplinkThroughputBps\":%.2f,\"obstructionStats\":{\"fractionObstructed\":%.6f},\"gpsStats\":{\"gpsValid\":%s,\"gpsSats\":%d},\"boresightAzimuthDeg\":%.2f,\"boresightElevationDeg\":%.2f}}\n",
                               (unsigned long long)api_version, uptime, pop_latency, drop_rate, dl, ul, obstruct, gps_valid?"true":"false", gps_sats, bore_az, bore_el);
                    else
                        printf("{\"dishGetStatus\":{\"deviceState\":{\"uptimeS\":%.0f},\"popPingLatencyMs\":%.2f,\"popPingDropRate\":%.3f,\"downlinkThroughputBps\":%.2f,\"uplinkThroughputBps\":%.2f,\"obstructionStats\":{\"fractionObstructed\":%.6f},\"gpsStats\":{\"gpsValid\":%s,\"gpsSats\":%d},\"boresightAzimuthDeg\":%.2f,\"boresightElevationDeg\":%.2f}}\n",
                               uptime, pop_latency, drop_rate, dl, ul, obstruct, gps_valid?"true":"false", gps_sats, bore_az, bore_el);
                    handled = 1; break;
                }
                if (strcmp(method, "get_location") == 0) {
                    pb_cursor_t d; pb_cursor_init(&d, ld, ldlen);
                    double lat=0.0, lon=0.0, alt=0.0; uint64_t source=0;
                    while (d.pos < d.length) {
                        uint32_t ff; pb_wire_type_t wtt; if (pb_decode_key(&d,&ff,&wtt)!=0) break;
                        if (ff == 1 && wtt == PB_WIRE_LEN) { const unsigned char *lla; size_t llalen; if(pb_decode_length_delimited(&d,&lla,&llalen)!=0) break; pb_cursor_t e; pb_cursor_init(&e,lla,llalen); while(e.pos<e.length){ uint32_t f3; pb_wire_type_t wt3; if(pb_decode_key(&e,&f3,&wt3)!=0) break; if(wt3==PB_WIRE_64BIT && (f3==1||f3==2||f3==3)){ const unsigned char *p; if(pb_read_bytes(&e,8,&p)==0){ double dv; memcpy(&dv,p,8); if(f3==1)lat=dv; else if(f3==2)lon=dv; else alt=dv; } } else { if(pb_skip_value(&e,wt3)!=0) break; } } }
                        else if (ff == 3 && wtt == PB_WIRE_VARINT) { if(pb_decode_varint(&d,&source)!=0) break; }
                        else { if (pb_skip_value(&d,wtt)!=0) break; }
                    }
                    if (have_api_version) {
                        const char *src = NULL; if (source == 10) src = "GNC_STATIC";
                        if (src) printf("{\"apiVersion\":\"%llu\",\"getLocation\":{\"lla\":{\"lat\":%.14f,\"lon\":%.14f,\"alt\":%.8f},\"source\":\"%s\"}}\n", (unsigned long long)api_version, lat, lon, alt, src);
                        else printf("{\"apiVersion\":\"%llu\",\"getLocation\":{\"lla\":{\"lat\":%.14f,\"lon\":%.14f,\"alt\":%.8f},\"sourceCode\":%llu}}\n", (unsigned long long)api_version, lat, lon, alt, (unsigned long long)source);
                    } else {
                        const char *src = NULL; if (source == 10) src = "GNC_STATIC";
                        if (src) printf("{\"getLocation\":{\"lla\":{\"lat\":%.14f,\"lon\":%.14f,\"alt\":%.8f},\"source\":\"%s\"}}\n", lat, lon, alt, src);
                        else printf("{\"getLocation\":{\"lla\":{\"lat\":%.14f,\"lon\":%.14f,\"alt\":%.8f},\"sourceCode\":%llu}}\n", lat, lon, alt, (unsigned long long)source);
                    }
                    handled = 1; break;
                }
                if (strcmp(method, "get_history") == 0) {
                    pb_cursor_t d; pb_cursor_init(&d, ld, ldlen);
                    char current[32]={0}; const unsigned char *series=NULL; size_t series_len=0; int have_series=0;
                    while (d.pos < d.length) {
                        uint32_t df; pb_wire_type_t dw; if (pb_decode_key(&d,&df,&dw)!=0) break;
                        if (df==1 && dw==PB_WIRE_LEN){ const unsigned char *s; size_t sl; if(pb_decode_length_delimited(&d,&s,&sl)==0){ size_t n=sl<sizeof(current)-1?sl:sizeof(current)-1; memcpy(current,s,n); current[n]='\0'; } }
                        else if (dw==PB_WIRE_LEN && !have_series){ if(pb_decode_length_delimited(&d,&series,&series_len)==0){ if(series_len%4==0) have_series=1; } }
                        else { if (pb_skip_value(&d,dw)!=0) break; }
                    }
                    if (have_api_version) printf("{\"apiVersion\":\"%llu\",\"dishGetHistory\":{\"current\":\"%s\",\"popPingDropRate\":[", (unsigned long long)api_version, current);
                    else printf("{\"dishGetHistory\":{\"current\":\"%s\",\"popPingDropRate\":[", current);
                    if (have_series) { size_t n=series_len/4; for(size_t i=0;i<n;i++){ float fv; memcpy(&fv, series+i*4, 4); if (i) printf(","); printf("%g", fv); } }
                    printf("]}}\n");
                    handled = 1; break;
                }
                if (strcmp(method, "get_diagnostics") == 0) {
                    pb_cursor_t d; pb_cursor_init(&d, ld, ldlen);
                    char id[128]={0}, hw[64]={0}, sw[64]={0}; int have_loc=0; int loc_enabled=0; double lat=0, lon=0, altm=0, gpstime=0; int have_align=0; double b_az=0, b_el=0, db_az=0, db_el=0; uint64_t utcOffsetS=0; int have_utc=0;
                    while (d.pos < d.length) {
                        uint32_t df; pb_wire_type_t dw; if (pb_decode_key(&d,&df,&dw)!=0) break;
                        if (df == 1 && dw == PB_WIRE_LEN) { const unsigned char *s; size_t sl; if(pb_decode_length_delimited(&d,&s,&sl)==0){ size_t n=sl<sizeof(id)-1?sl:sizeof(id)-1; memcpy(id,s,n); id[n]='\0'; } }
                        else if (df == 2 && dw == PB_WIRE_LEN) { const unsigned char *s; size_t sl; if(pb_decode_length_delimited(&d,&s,&sl)==0){ size_t n=sl<sizeof(hw)-1?sl:sizeof(hw)-1; memcpy(hw,s,n); hw[n]='\0'; } }
                        else if (df == 3 && dw == PB_WIRE_LEN) { const unsigned char *s; size_t sl; if(pb_decode_length_delimited(&d,&s,&sl)==0){ size_t n=sl<sizeof(sw)-1?sl:sizeof(sw)-1; memcpy(sw,s,n); sw[n]='\0'; } }
                        else if (!have_utc && dw == PB_WIRE_VARINT) { uint64_t v; if(pb_decode_varint(&d,&v)==0){ utcOffsetS=v; have_utc=1; } }
                        else if (df == 10 && dw == PB_WIRE_LEN) { const unsigned char *loc; size_t loclen; if(pb_decode_length_delimited(&d,&loc,&loclen)==0){ pb_cursor_t e; pb_cursor_init(&e,loc,loclen); while(e.pos<e.length){ uint32_t lf; pb_wire_type_t lw; if(pb_decode_key(&e,&lf,&lw)!=0) break; if(lf==1 && lw==PB_WIRE_VARINT){ uint64_t v; if(pb_decode_varint(&e,&v)==0) loc_enabled=(v!=0); }
                                else if ((lf==2||lf==3||lf==4||lf==5) && lw==PB_WIRE_64BIT){ const unsigned char *p; if(pb_read_bytes(&e,8,&p)==0){ double dv; memcpy(&dv,p,8); if(lf==2)lat=dv; else if(lf==3)lon=dv; else if(lf==4)altm=dv; else gpstime=dv; have_loc=1; } } else { if(pb_skip_value(&e,lw)!=0) break; } } } }
                        else if (df == 11 && dw == PB_WIRE_LEN) { const unsigned char *as; size_t aslen; if(pb_decode_length_delimited(&d,&as,&aslen)==0){ pb_cursor_t e; pb_cursor_init(&e,as,aslen); while(e.pos<e.length){ uint32_t af; pb_wire_type_t aw; if(pb_decode_key(&e,&af,&aw)!=0) break; if(aw==PB_WIRE_32BIT){ const unsigned char *p; if(pb_read_bytes(&e,4,&p)==0){ float fv; memcpy(&fv,p,4); if(af==1)b_az=fv; else if(af==2)b_el=fv; else if(af==3)db_az=fv; else if(af==4)db_el=fv; have_align=1; } } else { if(pb_skip_value(&e,aw)!=0) break; } } } }
                        else { if (pb_skip_value(&d,dw)!=0) break; }
                    }
                    if (have_api_version) {
                        printf("{\"apiVersion\":\"%llu\",\"dishGetDiagnostics\":{\"id\":\"%s\",\"hardwareVersion\":\"%s\",\"softwareVersion\":\"%s\"",
                               (unsigned long long)api_version, id, hw, sw);
                    } else {
                        printf("{\"dishGetDiagnostics\":{\"id\":\"%s\",\"hardwareVersion\":\"%s\",\"softwareVersion\":\"%s\"",
                               id, hw, sw);
                    }
                    if (have_loc) {
                        printf(",\"location\":{\"enabled\":%s,\"latitude\":%.9f,\"longitude\":%.9f,\"altitudeMeters\":%.9f,\"gpsTimeS\":%.10g}",
                               loc_enabled?"true":"false", lat, lon, altm, gpstime);
                    }
                    if (have_align) {
                        printf(",\"alignmentStats\":{\"boresightAzimuthDeg\":%.7g,\"boresightElevationDeg\":%.7g,\"desiredBoresightAzimuthDeg\":%.7g,\"desiredBoresightElevationDeg\":%.7g}",
                               b_az, b_el, db_az, db_el);
                    }
                    printf("}}\n");
                    handled = 1; break;
                }
                if (strcmp(method, "dish_get_obstruction_map") == 0) {
                    pb_cursor_t d; pb_cursor_init(&d, ld, ldlen);
                    uint64_t numRows=0, numCols=0; int have_rows=0, have_cols=0; const unsigned char *snrp=NULL; size_t snrlen=0; int have_snr=0;
                    while (d.pos < d.length) {
                        uint32_t df; pb_wire_type_t dw; if (pb_decode_key(&d,&df,&dw)!=0) break;
                        if (df==1 && dw==PB_WIRE_VARINT){ uint64_t v; if(pb_decode_varint(&d,&v)==0){ numRows=v; have_rows=1; } }
                        else if (df==2 && dw==PB_WIRE_VARINT){ uint64_t v; if(pb_decode_varint(&d,&v)==0){ numCols=v; have_cols=1; } }
                        else if (df==3 && dw==PB_WIRE_LEN){ if(pb_decode_length_delimited(&d,&snrp,&snrlen)==0) have_snr=1; }
                        else { if (pb_skip_value(&d,dw)!=0) break; }
                    }
                    if (have_api_version) printf("{\"apiVersion\":\"%llu\",\"dishGetObstructionMap\":{\"numRows\":%llu,\"numCols\":%llu,\"snr\":[", (unsigned long long)api_version, (unsigned long long)numRows, (unsigned long long)numCols);
                    else printf("{\"dishGetObstructionMap\":{\"numRows\":%llu,\"numCols\":%llu,\"snr\":[", (unsigned long long)numRows, (unsigned long long)numCols);
                    if (have_snr && snrlen >= 4) { size_t n=snrlen/4; for(size_t i=0;i<n;i++){ float fv; memcpy(&fv, snrp+i*4, 4); int iv = (int)(fv < 0 ? fv - 0.5f : fv + 0.5f); if(i) printf(","); printf("%d", iv); } }
                    printf("]}}\n");
                    handled = 1; break;
                }
                if (strcmp(method, "dish_clear_obstruction_map") == 0) {
                    if (have_api_version) printf("{\"apiVersion\":\"%llu\",\"dishClearObstructionMap\":{}}\n", (unsigned long long)api_version);
                    else printf("{\"dishClearObstructionMap\":{}}\n");
                    handled = 1; break;
                }
                if (strcmp(method, "dish_get_config") == 0) {
                    // Expect DishGetConfigResponse (2011) with dish_config (1)
                    pb_cursor_t d; pb_cursor_init(&d, ld, ldlen);
                    int printed = 0; while (d.pos < d.length) { uint32_t df; pb_wire_type_t dw; if (pb_decode_key(&d,&df,&dw)!=0) break; if (df==1 && dw==PB_WIRE_LEN) { const unsigned char *dc; size_t dclen; if(pb_decode_length_delimited(&d,&dc,&dclen)!=0) break; pb_cursor_t e; pb_cursor_init(&e, dc, dclen);
                            int have_snow=0, have_loc=0, have_level=0, have_pss=0, have_psd=0, have_psm=0, have_def=0, have_asset=0, have_reboot=0;
                            uint64_t snow=0, loc=0, level=0, pss=0, psd=0, psm=0, def=0, asset=0, reboot=0;
                            int a_snow=-1,a_loc=-1,a_level=-1,a_pss=-1,a_psd=-1,a_psm=-1,a_def=-1,a_asset=-1,a_reboot=-1;
                            while (e.pos < e.length) { uint32_t ef; pb_wire_type_t ew; if (pb_decode_key(&e,&ef,&ew)!=0) break; if (ew==PB_WIRE_VARINT) { uint64_t v; if (pb_decode_varint(&e,&v)!=0) break; if(ef==1){snow=v;have_snow=1;} else if(ef==2){loc=v;have_loc=1;} else if(ef==3){level=v;have_level=1;} else if(ef==4){pss=v;have_pss=1;} else if(ef==5){psd=v;have_psd=1;} else if(ef==6){psm=v;have_psm=1;} else if(ef==7){def=v;have_def=1;} else if(ef==8){asset=v;have_asset=1;} else if(ef==9){reboot=v;have_reboot=1;} else if(ef==1001){a_snow=(int)v;} else if(ef==2001){a_loc=(int)v;} else if(ef==3001){a_level=(int)v;} else if(ef==4001){a_pss=(int)v;} else if(ef==5001){a_psd=(int)v;} else if(ef==6001){a_psm=(int)v;} else if(ef==7001){a_def=(int)v;} else if(ef==8001){a_asset=(int)v;} else if(ef==9001){a_reboot=(int)v;} else { /* skip unknown varint */ } } else { if (pb_skip_value(&e, ew)!=0) break; } }
                            const char *snow_s = (snow==1?"ALWAYS_ON":(snow==2?"ALWAYS_OFF":"AUTO"));
                            const char *loc_s = (loc==1?"LOCAL":"NONE");
                            const char *level_s = (level==1?"FORCE_LEVEL":"TILT_LIKE_NORMAL");
                            if (have_api_version) printf("{\"apiVersion\":\"%llu\",\"dishGetConfig\":{\"dishConfig\":{", (unsigned long long)api_version); else printf("{\"dishGetConfig\":{\"dishConfig\":{");
                            int first = 1;
                            if (have_snow) EMIT_KV(first, "\"snowMeltMode\":\"%s\"", snow_s);
                            if (have_loc) EMIT_KV(first, "\"locationRequestMode\":\"%s\"", loc_s);
                            if (have_level) EMIT_KV(first, "\"levelDishMode\":\"%s\"", level_s);
                            if (have_pss) EMIT_KV(first, "\"powerSaveStartMinutes\":%llu", (unsigned long long)pss);
                            if (have_psd) EMIT_KV(first, "\"powerSaveDurationMinutes\":%llu", (unsigned long long)psd);
                            if (have_psm) EMIT_KV(first, "\"powerSaveMode\":%s", psm?"true":"false");
                            if (have_def) EMIT_KV(first, "\"swupdateThreeDayDeferralEnabled\":%s", def?"true":"false");
                            if (have_asset) EMIT_KV(first, "\"assetClass\":%llu", (unsigned long long)asset);
                            if (have_reboot) EMIT_KV(first, "\"swupdateRebootHour\":%llu", (unsigned long long)reboot);
                            if (a_snow!=-1) EMIT_KV(first, "\"applySnowMeltMode\":%s", a_snow?"true":"false");
                            if (a_loc!=-1) EMIT_KV(first, "\"applyLocationRequestMode\":%s", a_loc?"true":"false");
                            if (a_level!=-1) EMIT_KV(first, "\"applyLevelDishMode\":%s", a_level?"true":"false");
                            if (a_pss!=-1) EMIT_KV(first, "\"applyPowerSaveStartMinutes\":%s", a_pss?"true":"false");
                            if (a_psd!=-1) EMIT_KV(first, "\"applyPowerSaveDurationMinutes\":%s", a_psd?"true":"false");
                            if (a_psm!=-1) EMIT_KV(first, "\"applyPowerSaveMode\":%s", a_psm?"true":"false");
                            if (a_def!=-1) EMIT_KV(first, "\"applySwupdateThreeDayDeferralEnabled\":%s", a_def?"true":"false");
                            if (a_asset!=-1) EMIT_KV(first, "\"applyAssetClass\":%s", a_asset?"true":"false");
                            if (a_reboot!=-1) EMIT_KV(first, "\"applySwupdateRebootHour\":%s", a_reboot?"true":"false");
                            printf("}}}\n"); printed = 1; break; }
                        else { if (pb_skip_value(&d,dw)!=0) break; }
                    }
                    if (printed) { handled = 1; break; }
                }
                if (strcmp(method, "dish_set_config") == 0) {
                    // Response likely empty; just print wrapper
                    if (have_api_version) printf("{\\\"apiVersion\\\":\\\"%llu\\\",\\\"dishSetConfig\\\":{}}\n", (unsigned long long)api_version);
                    else printf("{\\\"dishSetConfig\\\":{}}\n");
                    handled = 1; break;
                }
            } else {
                if (pb_skip_value(&c, wt) != 0) break;
            }
        }
        if (!handled) {
            // Fallback: attempt to fetch protoset via reflection for future debugging
            fprintf(stderr, "No parser for response; consider fetching protoset via reflection.\n");
            printf("First gRPC message length: %zu\n", msg_len);
            size_t show = msg_len < 64 ? msg_len : 64; for (size_t i=0;i<show;i++) printf("%02x ", msg[i]); printf("\n");
        }
    } else {
        if (strcmp(method, "dish_set_config") == 0) {
            // Some firmwares return empty response body; treat as success
            printf("{\"dishSetConfig\":{}}\n");
        } else {
            fprintf(stderr, "Failed to extract first gRPC message\n");
        }
    }

    curl_slist_free_all(hdr);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
}


