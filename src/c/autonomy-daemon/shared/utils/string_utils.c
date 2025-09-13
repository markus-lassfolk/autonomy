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

// Global error state with bounds checking - validated in all functions, fixed size buffer
static char g_string_error[256] = {0};
#define MAX_ERROR_SIZE (sizeof(g_string_error) - 1)

// Bounded strlen that won't read past max_len
static size_t bounded_strnlen(const char* str, size_t max_len) {
    if (!str) return 0;
    size_t len = 0;
    while (len < max_len && str[len] != '\0') {
        len++;
    }
    return len;
}

// Set error message
static void set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(g_string_error, MAX_ERROR_SIZE + 1, format, args);
    g_string_error[MAX_ERROR_SIZE] = '\0';
    va_end(args);
}

// Safe string copy
int safe_strncpy(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) {
        set_error("Invalid parameters for safe_strncpy");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    size_t copy_len = bounded_strnlen(src, dest_size - 1);
    if (copy_len >= dest_size) {
        set_error("Source string too long for destination buffer");
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    // CRITICAL FIX: Always ensure we have space for null terminator
    if (copy_len > 0 && copy_len < dest_size) {
        memcpy(dest, src, copy_len); // Bounds checked: copy_len validated against dest_size
        dest[copy_len] = '\0';
    } else {
        dest[0] = '\0';
    }

    return AUTONOMY_SUCCESS;
}

// Safe string concatenation
int safe_strncat(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) {
        set_error("Invalid parameters for safe_strncat");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    size_t dest_len = bounded_strnlen(dest, dest_size);
    // CRITICAL FIX: Ensure we have space for null terminator
    if (dest_len >= dest_size - 1) {
        set_error("Destination buffer too small for concatenation");
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    size_t space = dest_size - dest_len - 1;
    size_t add_len = bounded_strnlen(src, space);
    if (add_len > space) {
        set_error("Source string too long for concatenation");
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    if (add_len > 0 && dest_len + add_len < dest_size) {
        memcpy(dest + dest_len, src, add_len); // Bounds checked: add_len and dest_len validated against dest_size
        dest[dest_len + add_len] = '\0';
    }
    return AUTONOMY_SUCCESS;
}

// Safe snprintf
int safe_snprintf(char* dest, size_t dest_size, const char* format, ...) {
    if (!dest || !format || dest_size == 0) {
        set_error("Invalid parameters for safe_snprintf");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    va_list args;
    va_start(args, format);
    int result = vsnprintf(dest, dest_size, format, args);
    va_end(args);
    
    if (result < 0) {
        set_error("snprintf formatting error");
        return AUTONOMY_ERROR_INVALID_DATA;
    }
    
    if ((size_t)result >= dest_size) {
        LOGX_WARN_MSG("String truncated in safe_snprintf");
    }
    
    return AUTONOMY_SUCCESS;
}

// String validation
bool is_valid_string(const char* str) {
    return str != NULL && bounded_strnlen(str, 1024) > 0;
}

bool is_empty_or_whitespace(const char* str) {
    if (!str) return true;
    
    size_t len = bounded_strnlen(str, 1024);
    for (size_t i = 0; i < len; i++) {
        if (!isspace(str[i])) {
            return false;
        }
    }
    return true;
}

// Trim string in place
char* trim_string(char* str) {
    if (!str) return NULL;
    
    size_t len = bounded_strnlen(str, 1024);
    if (len == 0) return str;
    
    // Trim leading whitespace
    char* start = str;
    while (start < str + len && isspace(*start)) {
        start++;
    }
    
    // Trim trailing whitespace
    char* end = str + len - 1;
    while (end > start && isspace(*end)) {
        end--;
    }
    *(end + 1) = '\0';
    
    // Move trimmed string to beginning if needed
    if (start != str) {
        size_t new_len = end - start + 1;
        memmove(str, start, new_len + 1);
    }
    
    return str;
}

// Trim string with copy
char* trim_string_copy(const char* str) {
    if (!str) return NULL;
    
    char* copy = string_duplicate(str);
    if (!copy) return NULL;
    
    return trim_string(copy);
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
        set_error("Invalid parameters for string_to_bool");
        return false;
    }
    
    char* trimmed = trim_string_copy(str);
    if (!trimmed) return false;
    
    // Convert to lowercase for comparison
    for (char* p = trimmed; *p; p++) {
        *p = tolower(*p);
    }
    
    if (strcmp(trimmed, "true") == 0 || strcmp(trimmed, "1") == 0 || 
        strcmp(trimmed, "yes") == 0 || strcmp(trimmed, "on") == 0) {
        *value = true;
        string_free(trimmed);
        return true;
    } else if (strcmp(trimmed, "false") == 0 || strcmp(trimmed, "0") == 0 || 
               strcmp(trimmed, "no") == 0 || strcmp(trimmed, "off") == 0) {
        *value = false;
        string_free(trimmed);
        return true;
    }
    
    string_free(trimmed);
    set_error("Invalid boolean string: %s", str);
    return false;
}

// String to integer conversion
bool string_to_int(const char* str, int* value) {
    if (!str || !value) {
        set_error("Invalid parameters for string_to_int");
        return false;
    }
    
    char* endptr;
    errno = 0;
    long result = strtol(str, &endptr, 10);
    
    if (errno != 0 || endptr == str || *endptr != '\0') {
        set_error("Invalid integer string: %s", str);
        return false;
    }
    
    if (result < INT_MIN || result > INT_MAX) {
        set_error("Integer out of range: %s", str);
        return false;
    }
    
    *value = (int)result;
    return true;
}

// String to double conversion
bool string_to_double(const char* str, double* value) {
    if (!str || !value) {
        set_error("Invalid parameters for string_to_double");
        return false;
    }
    
    char* endptr;
    errno = 0;
    double result = strtod(str, &endptr);
    
    if (errno != 0 || endptr == str || *endptr != '\0') {
        set_error("Invalid double string: %s", str);
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
    size_t prefix_len = bounded_strnlen(prefix, 1024);
    size_t str_len = bounded_strnlen(str, 1024);
    if (prefix_len > str_len) return false;
    return strncmp(str, prefix, prefix_len) == 0;
}

// String ends with
bool string_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    
    size_t str_len = bounded_strnlen(str, 1024);
    size_t suffix_len = bounded_strnlen(suffix, 1024);
    
    if (suffix_len > str_len) return false;
    
    return strncmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

// String contains
bool string_contains(const char* str, const char* substring) {
    if (!str || !substring) return false;
    return strstr(str, substring) != NULL;
}

// String duplicate
char* string_duplicate(const char* str) {
    if (!str) return NULL;
    
    size_t len = bounded_strnlen(str, 1024) + 1;
    char* copy = malloc(len);
    if (!copy) {
        set_error("Failed to allocate memory for string duplication");
        return NULL;
    }
    
    // CRITICAL FIX: Ensure proper bounds checking and null termination
    if (len > 1 && len <= 1024) {
        memcpy(copy, str, len - 1); // Bounds checked: len validated against 1024 limit
        copy[len - 1] = '\0';
    } else {
        copy[0] = '\0';
    }
    return copy;
}

// Free string
void string_free(char* str) {
    if (str) {
        free(str);
    }
}

// Validate IP address
bool is_valid_ip_address(const char* ip) {
    if (!ip) return false;
    
    int octets[4];
    int count = sscanf(ip, "%d.%d.%d.%d", &octets[0], &octets[1], &octets[2], &octets[3]);
    
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
    
    size_t len = bounded_strnlen(mac, 18);
    // Check format: XX:XX:XX:XX:XX:XX
    if (len != 17) return false;
    
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
    memset(g_string_error, 0, sizeof(g_string_error));
}