#ifndef WEBHOOK_CLIENT_H
#define WEBHOOK_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Webhook client for sending HTTP notifications

// Webhook configuration
typedef struct {
    char url[512];
    char method[16];
    char content_type[64];
    char headers[1024];
    int timeout_seconds;
    int max_retries;
    bool verify_ssl;
    char auth_token[256];
} webhook_config_t;

// Webhook response
typedef struct {
    int status_code;
    char response_body[4096];
    int response_size;
    time_t response_time;
    bool success;
} webhook_response_t;

// Function declarations
int webhook_client_init(const webhook_config_t *config);
void webhook_client_cleanup(void);
int webhook_client_send(const char *payload, webhook_response_t *response);
int webhook_client_send_async(const char *payload, void (*callback)(webhook_response_t *response, void *user_data), void *user_data);
int webhook_client_test_connection(void);

#endif // WEBHOOK_CLIENT_H
