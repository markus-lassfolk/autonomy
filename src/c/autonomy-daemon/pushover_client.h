#ifndef PUSHOVER_CLIENT_H
#define PUSHOVER_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Pushover client for sending push notifications

// Pushover configuration
typedef struct {
    char api_token[128];
    char user_key[128];
    char device[64];
    char sound[32];
    int priority;
    int retry_seconds;
    int expire_seconds;
    char url[512];
    char url_title[128];
} pushover_config_t;

// Pushover message
typedef struct {
    char message[1024];
    char title[256];
    char device[64];
    char sound[32];
    int priority;
    int retry_seconds;
    int expire_seconds;
    char url[512];
    char url_title[128];
} pushover_message_t;

// Function declarations
int pushover_client_init(const pushover_config_t *config);
void pushover_client_cleanup(void);
int pushover_client_send(const pushover_message_t *message);
int pushover_client_send_async(const pushover_message_t *message, void (*callback)(bool success, void *user_data), void *user_data);
int pushover_client_test_connection(void);

#endif // PUSHOVER_CLIENT_H
