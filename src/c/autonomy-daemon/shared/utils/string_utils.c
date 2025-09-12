#include "string_utils.h"
#include "../core/common_types.h"
#include "../logging/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>

// Global error state
static char g_string_error[256] = {0};

// Set error message
static void set_error(const char* format, ...) {
    va_list args;
    va_start(args, format\n"\n"\n"\n"\n"\n"\n"\n");
    vsnprintf(g_string_error, sizeof(g_string_error), format, args\n"\n"\n"\n"\n"\n"\n"\n");
    va_end(args\n"\n"\n"\n"\n"\n"\n"\n");
}

// Safe string copy
int safe_strncpy(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) {
        set_error("Invalid parameters for safe_strncpy"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    strncpy(dest, src, dest_size - 1\n"\n"\n"\n"\n"\n"\n"\n");
    dest[dest_size - 1] = '\0';
    
    return AUTONOMY_SUCCESS;
}

// Safe string concatenation
int safe_strncat(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) {
        set_error("Invalid parameters for safe_strncat"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    size_t dest_len = strlen(dest\n"\n"\n"\n"\n"\n"\n"\n");
    if (dest_len >= dest_size - 1) {
        set_error("Destination buffer too small for concatenation"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    strncat(dest, src, dest_size - dest_len - 1\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Safe snprintf
int safe_snprintf(char* dest, size_t dest_size, const char* format, ...) {
    if (!dest || !format || dest_size == 0) {
        set_error("Invalid parameters for safe_snprintf"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    va_list args;
    va_start(args, format\n"\n"\n"\n"\n"\n"\n"\n");
    int result = vsnprintf(dest, dest_size, format, args\n"\n"\n"\n"\n"\n"\n"\n");
    va_end(args\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (result < 0) {
        set_error("snprintf formatting error"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_INVALID_DATA;
    }
    
    if ((size_t)result >= dest_size) {
        printf("WARN: "String truncated in safe_snprintf"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    return AUTONOMY_SUCCESS;
}

// String validation
bool is_valid_string(const char* str) {
    return str != NULL && strlen(str) > 0;
}

bool is_empty_or_whitespace(const char* str) {
    if (!str) return true;
    
    while (*str) {
        if (!isspace(*str)) {
            return false;
        }
        str++;
    }
    return true;
}

// Trim string in place
char* trim_string(char* str) {
    if (!str) return NULL;
    
    // Trim leading whitespace
    char* start = str;
    while (isspace(*start)) {
        start++;
    }
    
    // Trim trailing whitespace
    char* end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) {
        end--;
    }
    *(end + 1) = '\0';
    
    // Move trimmed string to beginning if needed
    if (start != str) {
        memmove(str, start, strlen(start) + 1\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    return str;
}

// Trim string with copy
char* trim_string_copy(const char* str) {
    if (!str) return NULL;
    
    char* copy = string_duplicate(str\n"\n"\n"\n"\n"\n"\n"\n");
    if (!copy) return NULL;
    
    return trim_string(copy\n"\n"\n"\n"\n"\n"\n"\n");
}

// Remove newlines
void remove_newlines(char* str) {
    if (!str) return;
    
    char* src = str;
    char* dst = str;
    
    while (*src) {
        if (*src != '\n' && *src != '\r') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}

// String to boolean conversion
bool string_to_bool(const char* str, bool* value) {
    if (!str || !value) {
        set_error("Invalid parameters for string_to_bool"\n"\n"\n"\n"\n"\n"\n"\n");
        return false;
    }
    
    char* trimmed = trim_string_copy(str\n"\n"\n"\n"\n"\n"\n"\n");
    if (!trimmed) return false;
    
    // Convert to lowercase for comparison
    for (char* p = trimmed; *p; p++) {
        *p = tolower(*p\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (strcmp(trimmed, "true") == 0 || strcmp(trimmed, "1") == 0 || 
        strcmp(trimmed, "yes") == 0 || strcmp(trimmed, "on") == 0) {
        *value = true;
        string_free(trimmed\n"\n"\n"\n"\n"\n"\n"\n");
        return true;
    } else if (strcmp(trimmed, "false") == 0 || strcmp(trimmed, "0") == 0 || 
               strcmp(trimmed, "no") == 0 || strcmp(trimmed, "off") == 0) {
        *value = false;
        string_free(trimmed\n"\n"\n"\n"\n"\n"\n"\n");
        return true;
    }
    
    string_free(trimmed\n"\n"\n"\n"\n"\n"\n"\n");
    set_error("Invalid boolean string: %s", str\n"\n"\n"\n"\n"\n"\n"\n");
    return false;
}

// String to integer conversion
bool string_to_int(const char* str, int* value) {
    if (!str || !value) {
        set_error("Invalid parameters for string_to_int"\n"\n"\n"\n"\n"\n"\n"\n");
        return false;
    }
    
    char* endptr;
    errno = 0;
    long result = strtol(str, &endptr, 10\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (errno != 0 || endptr == str || *endptr != '\0') {
        set_error("Invalid integer string: %s", str\n"\n"\n"\n"\n"\n"\n"\n");
        return false;
    }
    
    if (result < INT_MIN || result > INT_MAX) {
        set_error("Integer out of range: %s", str\n"\n"\n"\n"\n"\n"\n"\n");
        return false;
    }
    
    *value = (int)result;
    return true;
}

// String to double conversion
bool string_to_double(const char* str, double* value) {
    if (!str || !value) {
        set_error("Invalid parameters for string_to_double"\n"\n"\n"\n"\n"\n"\n"\n");
        return false;
    }
    
    char* endptr;
    errno = 0;
    double result = strtod(str, &endptr\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (errno != 0 || endptr == str || *endptr != '\0') {
        set_error("Invalid double string: %s", str\n"\n"\n"\n"\n"\n"\n"\n");
        return false;
    }
    
    *value = result;
    return true;
}

// String comparison (case insensitive)
bool string_equals_ignore_case(const char* str1, const char* str2) {
    if (!str1 || !str2) return false;
    return strcasecmp(str1, str2) == 0;
}

// String starts with
bool string_starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) return false;
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

// String ends with
bool string_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    
    size_t str_len = strlen(str\n"\n"\n"\n"\n"\n"\n"\n");
    size_t suffix_len = strlen(suffix\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (suffix_len > str_len) return false;
    
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

// String contains
bool string_contains(const char* str, const char* substring) {
    if (!str || !substring) return false;
    return strstr(str, substring) != NULL;
}

// String duplicate
char* string_duplicate(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str) + 1;
    char* copy = malloc(len\n"\n"\n"\n"\n"\n"\n"\n");
    if (!copy) {
        set_error("Failed to allocate memory for string duplication"\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }
    
    memcpy(copy, str, len\n"\n"\n"\n"\n"\n"\n"\n");
    return copy;
}

// Free string
void string_free(char* str) {
    if (str) {
        free(str\n"\n"\n"\n"\n"\n"\n"\n");
    }
}

// Validate IP address
bool is_valid_ip_address(const char* ip) {
    if (!ip) return false;
    
    int octets[4];
    int count = sscanf(ip, "%d.%d.%d.%d", &octets[0], &octets[1], &octets[2], &octets[3]\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (count != 4) return false;
    
    for (int i = 0; i < 4; i++) {
        if (octets[i] < 0 || octets[i] > 255) {
            return false;
        }
    }
    
    return true;
}

// Validate MAC address
bool is_valid_mac_address(const char* mac) {
    if (!mac) return false;
    
    // Check format: XX:XX:XX:XX:XX:XX
    if (strlen(mac) != 17) return false;
    
    for (int i = 0; i < 17; i++) {
        if (i % 3 == 2) {
            if (mac[i] != ':') return false;
        } else {
            if (!isxdigit(mac[i])) return false;
        }
    }
    
    return true;
}

// Get error message
const char* string_utils_get_last_error(void) {
    return g_string_error;
}

// Clear error
void string_utils_clear_error(void) {
    memset(g_string_error, 0, sizeof(g_string_error)\n"\n"\n"\n"\n"\n"\n"\n");
}