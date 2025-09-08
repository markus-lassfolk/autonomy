#ifndef AUTONOMY_CONSTANTS_H
#define AUTONOMY_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

// Duration window constants for consistent usage across the system
#define DURATION_WINDOW_COUNT 11

// Duration window labels
extern const char* DURATION_WINDOWS[DURATION_WINDOW_COUNT];

// Duration window values in seconds (for calculations)
extern const int DURATION_WINDOW_VALUES[DURATION_WINDOW_COUNT];

// Duration window ranges (min, max) in seconds
extern const int DURATION_WINDOW_RANGES[DURATION_WINDOW_COUNT][2];

// Function to get duration window index from seconds
int get_duration_window_index(int seconds);

// Function to get duration window label from seconds
const char* get_duration_window_label(int seconds);

// Function to get duration window range from index
void get_duration_window_range(int index, int *min_seconds, int *max_seconds);

#ifdef __cplusplus
}
#endif

#endif // AUTONOMY_CONSTANTS_H