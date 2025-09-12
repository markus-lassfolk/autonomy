#ifndef SHARED_STRING_UTILS_H
#define SHARED_STRING_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

// Shared string utility functions
// Consolidates common string operations used across modules

// Safe string operations
int safe_strncpy(char* dest, const char* src, size_t dest_size);
int safe_strncat(char* dest, const char* src, size_t dest_size);
int safe_snprintf(char* dest, size_t dest_size, const char* format, ...);

// String validation and sanitization
bool is_valid_string(const char* str);
bool is_empty_or_whitespace(const char* str);
char* trim_string(char* str);
char* trim_string_copy(const char* str);
void remove_newlines(char* str);
void remove_extra_spaces(char* str);

// String parsing
bool string_to_bool(const char* str, bool* value);
bool string_to_int(const char* str, int* value);
bool string_to_double(const char* str, double* value);
bool string_to_long(const char* str, long* value);

// String formatting (TODO: implement as needed)

// String comparison
bool string_equals_ignore_case(const char* str1, const char* str2);
bool string_starts_with(const char* str, const char* prefix);
bool string_ends_with(const char* str, const char* suffix);
bool string_contains(const char* str, const char* substring);

// Basic validation utilities
bool is_valid_ip_address(const char* ip);
bool is_valid_mac_address(const char* mac);

// Memory management for strings
char* string_duplicate(const char* str);
void string_free(char* str);

// Constants
#define STRING_UTILS_MAX_LENGTH 4096
#define STRING_UTILS_MAX_TOKENS 256

// Error handling
const char* string_utils_get_last_error(void);
void string_utils_clear_error(void);

#endif // SHARED_STRING_UTILS_H