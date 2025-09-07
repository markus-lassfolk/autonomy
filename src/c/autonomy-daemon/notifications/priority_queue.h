#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include "notification_types.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// Priority queue item
typedef struct {
    notification_event_t event;
    notification_priority_t priority;
    time_t timestamp;
} priority_queue_item_t;

// Priority queue structure
typedef struct {
    priority_queue_item_t* items;
    int size;
    int max_size;
    pthread_mutex_t* mutex;
} priority_queue_t;

// Priority queue statistics
typedef struct {
    int current_size;
    int max_size;
    bool is_empty;
    bool is_full;
    notification_priority_t highest_priority;
    notification_priority_t lowest_priority;
    time_t oldest_timestamp;
    time_t newest_timestamp;
} priority_queue_stats_t;

// Initialize priority queue
int priority_queue_init(priority_queue_t* queue, int max_size);

// Clean up priority queue
void priority_queue_cleanup(priority_queue_t* queue);

// Push item to priority queue
int priority_queue_push(priority_queue_t* queue, const notification_event_t* event, 
                       notification_priority_t priority, time_t timestamp);

// Pop highest priority item from queue
int priority_queue_pop(priority_queue_t* queue, priority_queue_item_t* item);

// Peek at highest priority item without removing it
int priority_queue_peek(const priority_queue_t* queue, priority_queue_item_t* item);

// Get queue size
int priority_queue_size(const priority_queue_t* queue);

// Check if queue is empty
bool priority_queue_is_empty(const priority_queue_t* queue);

// Check if queue is full
bool priority_queue_is_full(const priority_queue_t* queue);

// Clear all items from queue
void priority_queue_clear(priority_queue_t* queue);

// Remove items older than specified timestamp
int priority_queue_remove_old(priority_queue_t* queue, time_t cutoff_time);

// Get statistics about the queue
void priority_queue_get_stats(const priority_queue_t* queue, priority_queue_stats_t* stats);

#endif // PRIORITY_QUEUE_H
