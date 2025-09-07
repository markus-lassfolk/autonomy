#include "priority_queue.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// Initialize priority queue
int priority_queue_init(priority_queue_t* queue, int max_size) {
    if (!queue || max_size <= 0) {
        return -1;
    }
    
    queue->items = malloc(max_size * sizeof(priority_queue_item_t));
    if (!queue->items) {
        return -1;
    }
    
    queue->max_size = max_size;
    queue->size = 0;
    queue->mutex = malloc(sizeof(pthread_mutex_t));
    if (!queue->mutex) {
        free(queue->items);
        return -1;
    }
    
    pthread_mutex_init(queue->mutex, NULL);
    return 0;
}

// Clean up priority queue
void priority_queue_cleanup(priority_queue_t* queue) {
    if (!queue) return;
    
    if (queue->mutex) {
        pthread_mutex_destroy(queue->mutex);
        free(queue->mutex);
    }
    
    if (queue->items) {
        free(queue->items);
    }
    
    queue->items = NULL;
    queue->mutex = NULL;
    queue->size = 0;
    queue->max_size = 0;
}

// Swap two items in the heap
static void swap_items(priority_queue_item_t* a, priority_queue_item_t* b) {
    priority_queue_item_t temp = *a;
    *a = *b;
    *b = temp;
}

// Heapify up (bubble up)
static void heapify_up(priority_queue_t* queue, int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        
        // Higher priority (lower number) items go to the top
        if (queue->items[parent].priority <= queue->items[index].priority) {
            break;
        }
        
        swap_items(&queue->items[parent], &queue->items[index]);
        index = parent;
    }
}

// Heapify down (bubble down)
static void heapify_down(priority_queue_t* queue, int index) {
    int size = queue->size;
    
    while (true) {
        int smallest = index;
        int left = 2 * index + 1;
        int right = 2 * index + 2;
        
        if (left < size && queue->items[left].priority < queue->items[smallest].priority) {
            smallest = left;
        }
        
        if (right < size && queue->items[right].priority < queue->items[smallest].priority) {
            smallest = right;
        }
        
        if (smallest == index) {
            break;
        }
        
        swap_items(&queue->items[index], &queue->items[smallest]);
        index = smallest;
    }
}

// Push item to priority queue
int priority_queue_push(priority_queue_t* queue, const notification_event_t* event, 
                       notification_priority_t priority, time_t timestamp) {
    if (!queue || !event || !queue->mutex) {
        return -1;
    }
    
    pthread_mutex_lock(queue->mutex);
    
    if (queue->size >= queue->max_size) {
        pthread_mutex_unlock(queue->mutex);
        return -1; // Queue full
    }
    
    // Add item to the end
    int index = queue->size;
    queue->items[index].priority = priority;
    queue->items[index].timestamp = timestamp;
    
    // Copy event data
    strncpy(queue->items[index].event.id, event->id, sizeof(queue->items[index].event.id) - 1);
    strncpy(queue->items[index].event.title, event->title, sizeof(queue->items[index].event.title) - 1);
    strncpy(queue->items[index].event.message, event->message, sizeof(queue->items[index].event.message) - 1);
    queue->items[index].event.type = event->type;
    queue->items[index].event.priority = event->priority;
    strncpy(queue->items[index].event.sound, event->sound, sizeof(queue->items[index].event.sound) - 1);
    strncpy(queue->items[index].event.url, event->url, sizeof(queue->items[index].event.url) - 1);
    strncpy(queue->items[index].event.url_title, event->url_title, sizeof(queue->items[index].event.url_title) - 1);
    queue->items[index].event.timestamp = event->timestamp;
    
    // Copy enhanced context data
    strncpy(queue->items[index].event.member_name, event->member_name, sizeof(queue->items[index].event.member_name) - 1);
    strncpy(queue->items[index].event.from_member, event->from_member, sizeof(queue->items[index].event.from_member) - 1);
    strncpy(queue->items[index].event.to_member, event->to_member, sizeof(queue->items[index].event.to_member) - 1);
    strncpy(queue->items[index].event.error_details, event->error_details, sizeof(queue->items[index].event.error_details) - 1);
    strncpy(queue->items[index].event.details_json, event->details_json, sizeof(queue->items[index].event.details_json) - 1);
    
    // Copy rich context features
    if (event->location) {
        queue->items[index].event.location = malloc(sizeof(notification_location_t));
        if (queue->items[index].event.location) {
            memcpy(queue->items[index].event.location, event->location, sizeof(notification_location_t));
        }
    } else {
        queue->items[index].event.location = NULL;
    }
    
    queue->items[index].event.duration = event->duration;
    queue->items[index].event.acknowledged = event->acknowledged;
    strncpy(queue->items[index].event.message_id, event->message_id, sizeof(queue->items[index].event.message_id) - 1);
    
    queue->size++;
    
    // Restore heap property
    heapify_up(queue, index);
    
    pthread_mutex_unlock(queue->mutex);
    return 0;
}

// Pop highest priority item from queue
int priority_queue_pop(priority_queue_t* queue, priority_queue_item_t* item) {
    if (!queue || !item || !queue->mutex) {
        return -1;
    }
    
    pthread_mutex_lock(queue->mutex);
    
    if (queue->size == 0) {
        pthread_mutex_unlock(queue->mutex);
        return -1; // Queue empty
    }
    
    // Copy the root item (highest priority)
    *item = queue->items[0];
    
    // Move last item to root
    queue->size--;
    if (queue->size > 0) {
        queue->items[0] = queue->items[queue->size];
        heapify_down(queue, 0);
    }
    
    pthread_mutex_unlock(queue->mutex);
    return 0;
}

// Peek at highest priority item without removing it
int priority_queue_peek(const priority_queue_t* queue, priority_queue_item_t* item) {
    if (!queue || !item || !queue->mutex) {
        return -1;
    }
    
    pthread_mutex_lock(queue->mutex);
    
    if (queue->size == 0) {
        pthread_mutex_unlock(queue->mutex);
        return -1; // Queue empty
    }
    
    *item = queue->items[0];
    
    pthread_mutex_unlock(queue->mutex);
    return 0;
}

// Get queue size
int priority_queue_size(const priority_queue_t* queue) {
    if (!queue || !queue->mutex) {
        return -1;
    }
    
    pthread_mutex_lock(queue->mutex);
    int size = queue->size;
    pthread_mutex_unlock(queue->mutex);
    
    return size;
}

// Check if queue is empty
bool priority_queue_is_empty(const priority_queue_t* queue) {
    return priority_queue_size(queue) == 0;
}

// Check if queue is full
bool priority_queue_is_full(const priority_queue_t* queue) {
    if (!queue) return true;
    return priority_queue_size(queue) >= queue->max_size;
}

// Clear all items from queue
void priority_queue_clear(priority_queue_t* queue) {
    if (!queue || !queue->mutex) return;
    
    pthread_mutex_lock(queue->mutex);
    
    // Free location data for all items
    for (int i = 0; i < queue->size; i++) {
        if (queue->items[i].event.location) {
            free(queue->items[i].event.location);
            queue->items[i].event.location = NULL;
        }
    }
    
    queue->size = 0;
    
    pthread_mutex_unlock(queue->mutex);
}

// Remove items older than specified timestamp
int priority_queue_remove_old(priority_queue_t* queue, time_t cutoff_time) {
    if (!queue || !queue->mutex) {
        return -1;
    }
    
    pthread_mutex_lock(queue->mutex);
    
    int removed_count = 0;
    int write_index = 0;
    
    for (int i = 0; i < queue->size; i++) {
        if (queue->items[i].timestamp >= cutoff_time) {
            // Keep this item
            if (write_index != i) {
                queue->items[write_index] = queue->items[i];
            }
            write_index++;
        } else {
            // Remove this item
            if (queue->items[i].event.location) {
                free(queue->items[i].event.location);
            }
            removed_count++;
        }
    }
    
    queue->size = write_index;
    
    // Restore heap property
    for (int i = queue->size / 2 - 1; i >= 0; i--) {
        heapify_down(queue, i);
    }
    
    pthread_mutex_unlock(queue->mutex);
    return removed_count;
}

// Get statistics about the queue
void priority_queue_get_stats(const priority_queue_t* queue, priority_queue_stats_t* stats) {
    if (!queue || !stats || !queue->mutex) return;
    
    pthread_mutex_lock(queue->mutex);
    
    stats->current_size = queue->size;
    stats->max_size = queue->max_size;
    stats->is_empty = (queue->size == 0);
    stats->is_full = (queue->size >= queue->max_size);
    
    if (queue->size > 0) {
        stats->highest_priority = queue->items[0].priority;
        stats->lowest_priority = queue->items[0].priority;
        stats->oldest_timestamp = queue->items[0].timestamp;
        stats->newest_timestamp = queue->items[0].timestamp;
        
        for (int i = 1; i < queue->size; i++) {
            if (queue->items[i].priority < stats->highest_priority) {
                stats->highest_priority = queue->items[i].priority;
            }
            if (queue->items[i].priority > stats->lowest_priority) {
                stats->lowest_priority = queue->items[i].priority;
            }
            if (queue->items[i].timestamp < stats->oldest_timestamp) {
                stats->oldest_timestamp = queue->items[i].timestamp;
            }
            if (queue->items[i].timestamp > stats->newest_timestamp) {
                stats->newest_timestamp = queue->items[i].timestamp;
            }
        }
    } else {
        stats->highest_priority = 0;
        stats->lowest_priority = 0;
        stats->oldest_timestamp = 0;
        stats->newest_timestamp = 0;
    }
    
    pthread_mutex_unlock(queue->mutex);
}
