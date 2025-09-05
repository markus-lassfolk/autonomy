#ifndef SLACK_CLIENT_H
#define SLACK_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Slack client for sending Slack webhook notifications

// Slack configuration
typedef struct {
    char webhook_url[512];
    char channel[64];
    char username[64];
    char icon_emoji[32];
    char icon_url[512];
    int timeout_seconds;
} slack_config_t;

// Slack message
typedef struct {
    char text[4000];
    char channel[64];
    char username[64];
    char icon_emoji[32];
    char icon_url[512];
    char attachments[4096]; // JSON string for attachments
} slack_message_t;

// Function declarations
int slack_client_init(const slack_config_t *config);
void slack_client_cleanup(void);
int slack_client_send(const slack_message_t *message);
int slack_client_send_async(const slack_message_t *message, void (*callback)(bool success, void *user_data), void *user_data);
int slack_client_test_connection(void);

#endif // SLACK_CLIENT_H
