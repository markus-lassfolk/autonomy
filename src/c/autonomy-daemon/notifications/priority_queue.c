#include "priority_queue.h"
#include "notification_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Initialize priority queue
int priority_queue_init(priority_queue_t *queue, int max_size) {
    if (!queue || max_size <= 0) {
        return -1;
    }
    
    queue->head = NULL;
    queue->size = 0;
    queue->max_size = max_size;
    
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        return -1;
    }
    
    if (pthread_cond_init(&queue->condition, NULL) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        return -1;
    }
    
    return 0;
}

// Clean up priority queue
void priority_queue_cleanup(priority_queue_t *queue) {
    if (!queue) return;
    
    pthread_mutex_lock(&queue->mutex);
    
    // Free all nodes
    queue_node_t *current = queue->head;
    while (current) {
        queue_node_t *next = current->next;
        if (current->data) {
            free(current->data);
        }
        free(current);
        current = next;
    }
    
    queue->head = NULL;
    queue->size = 0;
    
    pthread_mutex_unlock(&queue->mutex);
    pthread_cond_destroy(&queue->condition);
    pthread_mutex_destroy(&queue->mutex);
}

// Push data to queue with priority
int priority_queue_push(priority_queue_t *queue, void *data, int priority) {
    if (!queue || !data) {
        return -1;
    }
    
    pthread_mutex_lock(&queue->mutex);
    
    // Check if queue is full
    if (queue->size >= queue->max_size) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    
    // Create new node
    queue_node_t *new_node = malloc(sizeof(queue_node_t));
    if (!new_node) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    
    new_node->data = data;
    new_node->priority = priority;
    new_node->timestamp = time(NULL);
    new_node->next = NULL;
    
    // Insert in priority order (higher priority first)
    if (!queue->head || priority > queue->head->priority) {
        new_node->next = queue->head;
        queue->head = new_node;
    } else {
        queue_node_t *current = queue->head;
        while (current->next && current->next->priority >= priority) {
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
    }
    
    queue->size++;
    
    pthread_cond_signal(&queue->condition);
    pthread_mutex_unlock(&queue->mutex);
    
    return 0;
}

// Pop highest priority item
void* priority_queue_pop(priority_queue_t *queue) {
    if (!queue) {
        return NULL;
    }
    
    pthread_mutex_lock(&queue->mutex);
    
    if (!queue->head) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    
    queue_node_t *node = queue->head;
    void *data = node->data;
    
    queue->head = node->next;
    queue->size--;
    
    free(node);
    
    pthread_mutex_unlock(&queue->mutex);
    
    return data;
}

// Peek at highest priority item without removing
void* priority_queue_peek(priority_queue_t *queue) {
    if (!queue) {
        return NULL;
    }
    
    pthread_mutex_lock(&queue->mutex);
    
    void *data = NULL;
    if (queue->head) {
        data = queue->head->data;
    }
    
    pthread_mutex_unlock(&queue->mutex);
    
    return data;
}

// Get queue size
int priority_queue_size(priority_queue_t *queue) {
    if (!queue) {
        return -1;
    }
    
    pthread_mutex_lock(&queue->mutex);
    int size = queue->size;
    pthread_mutex_unlock(&queue->mutex);
    
    return size;
}

// Check if queue is empty
bool priority_queue_is_empty(priority_queue_t *queue) {
    if (!queue) {
        return true;
    }
    
    pthread_mutex_lock(&queue->mutex);
    bool empty = (queue->size == 0);
    pthread_mutex_unlock(&queue->mutex);
    
    return empty;
}

// Check if queue is full
bool priority_queue_is_full(priority_queue_t *queue) {
    if (!queue) {
        return true;
    }
    
    pthread_mutex_lock(&queue->mutex);
    bool full = (queue->size >= queue->max_size);
    pthread_mutex_unlock(&queue->mutex);
    
    return full;
}

// Clear all items from queue
int priority_queue_clear(priority_queue_t *queue) {
    if (!queue) {
        return -1;
    }
    
    pthread_mutex_lock(&queue->mutex);
    
    queue_node_t *current = queue->head;
    while (current) {
        queue_node_t *next = current->next;
        if (current->data) {
            free(current->data);
        }
        free(current);
        current = next;
    }
    
    queue->head = NULL;
    queue->size = 0;
    
    pthread_mutex_unlock(&queue->mutex);
    
    return 0;
}