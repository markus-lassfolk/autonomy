#ifndef TELEGRAM_CLIENT_H
#define TELEGRAM_CLIENT_H

#include "notification_types.h"
#include <stdbool.h>
#include <time.h>

// Telegram message
typedef struct {
    char chat_id[64];
    char text[4096];
    char parse_mode[16]; // "Markdown" or "HTML"
} telegram_message_t;

// Telegram configuration
typedef struct {
    bool enabled;
    char token[256];
    char chat_id[64];
    int timeout_seconds;
    int retry_attempts;
    int retry_delay_seconds;
    bool use_markdown;
    bool include_context;
} telegram_config_t;

// Telegram client status
typedef struct {
    bool enabled;
    char chat_id[64];
    int total_sent;
    int total_failed;
    int last_response_code;
    time_t last_sent_time;
    time_t last_error_time;
    char last_error[256];
} telegram_client_status_t;

// Telegram client structure
typedef struct {
    telegram_config_t config;
    telegram_client_status_t status;
} telegram_client_t;

// Initialize telegram client
int telegram_client_init(telegram_client_t* client, const telegram_config_t* config);

// Clean up telegram client
void telegram_client_cleanup(telegram_client_t* client);

// Send notification via Telegram
int telegram_client_send(telegram_client_t* client, const notification_event_t* event);

// Get telegram client status
void telegram_client_get_status(telegram_client_t* client, telegram_client_status_t* status);

// Create telegram message from notification event
void telegram_client_create_message(telegram_client_t* client, const notification_event_t* event, 
                                   telegram_message_t* message);

// Get priority emoji for notification priority
const char* telegram_client_get_priority_emoji(notification_priority_t priority);

// Get priority text for display
const char* telegram_client_get_priority_text(notification_priority_t priority);

// Escape markdown special characters
void telegram_client_escape_markdown(const char* input, char* output, size_t max_size);

#endif // TELEGRAM_CLIENT_H
