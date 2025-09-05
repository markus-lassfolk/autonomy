#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

// Priority queue implementation for notifications

// Queue node structure
typedef struct queue_node {
    void *data;
    int priority;
    time_t timestamp;
    struct queue_node *next;
} queue_node_t;

// Priority queue structure
typedef struct {
    queue_node_t *head;
    int size;
    int max_size;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
} priority_queue_t;

// Function declarations
int priority_queue_init(priority_queue_t *queue, int max_size);
void priority_queue_cleanup(priority_queue_t *queue);
int priority_queue_push(priority_queue_t *queue, void *data, int priority);
void* priority_queue_pop(priority_queue_t *queue);
void* priority_queue_peek(priority_queue_t *queue);
int priority_queue_size(priority_queue_t *queue);
bool priority_queue_is_empty(priority_queue_t *queue);
bool priority_queue_is_full(priority_queue_t *queue);
int priority_queue_clear(priority_queue_t *queue);

#endif // PRIORITY_QUEUE_H
