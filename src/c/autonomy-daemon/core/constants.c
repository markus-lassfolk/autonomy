#include "constants.h"
#include <string.h>

// Duration window labels
const char* DURATION_WINDOWS[DURATION_WINDOW_COUNT] = {
    "<2sec", "2-5sec", "5-10sec", "10-30sec", "30-60sec", 
    "1-2min", "2-5min", "5-15min", "15-60min", "1-4hours", ">4hours"
};

// Duration window values in seconds (representative values for calculations)
const int DURATION_WINDOW_VALUES[DURATION_WINDOW_COUNT] = {
    1,      // <2sec
    3,      // 2-5sec
    7,      // 5-10sec
    20,     // 10-30sec
    45,     // 30-60sec
    90,     // 1-2min
    210,    // 2-5min
    600,    // 5-15min
    2250,   // 15-60min
    9000,   // 1-4hours
    14400   // >4hours
};

// Duration window ranges (min, max) in seconds
const int DURATION_WINDOW_RANGES[DURATION_WINDOW_COUNT][2] = {
    {0, 2},        // <2sec
    {2, 5},        // 2-5sec
    {5, 10},       // 5-10sec
    {10, 30},      // 10-30sec
    {30, 60},      // 30-60sec
    {60, 120},     // 1-2min
    {120, 300},    // 2-5min
    {300, 900},    // 5-15min
    {900, 3600},   // 15-60min
    {3600, 14400}, // 1-4hours
    {14400, -1}    // >4hours (-1 means no upper limit)
};

// Get duration window index from seconds
int get_duration_window_index(int seconds) {
    if (seconds < 0) return 0;
    
    for (int i = 0; i < DURATION_WINDOW_COUNT; i++) {
        int min_sec = DURATION_WINDOW_RANGES[i][0];
        int max_sec = DURATION_WINDOW_RANGES[i][1];
        
        if (seconds >= min_sec && (max_sec == -1 || seconds < max_sec)) {
            return i;
        }
    }
    
    // Default to last window (>4hours)
    return DURATION_WINDOW_COUNT - 1;
}

// Get duration window label from seconds
const char* get_duration_window_label(int seconds) {
    int index = get_duration_window_index(seconds);
    return DURATION_WINDOWS[index];
}

// Get duration window range from index
void get_duration_window_range(int index, int *min_seconds, int *max_seconds) {
    if (index < 0 || index >= DURATION_WINDOW_COUNT || !min_seconds || !max_seconds) {
        if (min_seconds) *min_seconds = 0;
        if (max_seconds) *max_seconds = -1;
        return;
    }
    
    *min_seconds = DURATION_WINDOW_RANGES[index][0];
    *max_seconds = DURATION_WINDOW_RANGES[index][1];
}
