#ifndef DISCORD_CLIENT_H
#define DISCORD_CLIENT_H

#include "notification_types.h"
#include <stdbool.h>
#include <time.h>

// Discord embed field
typedef struct {
    char name[128];
    char value[256];
    bool inline_field;
} discord_embed_field_t;

// Discord embed
typedef struct {
    char title[256];
    char description[1024];
    int color;
    discord_embed_field_t fields[16];
    int field_count;
    char footer_text[128];
    char footer_icon_url[256];
    char timestamp[32];
} discord_embed_t;

// Discord message
typedef struct {
    char content[512];
    char username[128];
    char avatar_url[256];
    discord_embed_t embeds[4];
    int embed_count;
} discord_message_t;

// Discord configuration
typedef struct {
    bool enabled;
    char webhook_url[512];
    char username[128];
    char avatar_url[256];
    int timeout_seconds;
    int retry_attempts;
    int retry_delay_seconds;
    bool use_embeds;
    bool include_context;
} discord_config_t;

// Discord client status
typedef struct {
    bool enabled;
    char webhook_url[512];
    int total_sent;
    int total_failed;
    int last_response_code;
    time_t last_sent_time;
    time_t last_error_time;
    char last_error[256];
} discord_client_status_t;

// Discord client structure
typedef struct {
    discord_config_t config;
    discord_client_status_t status;
} discord_client_t;

// Initialize discord client
int discord_client_init(discord_client_t* client, const discord_config_t* config);

// Clean up discord client
void discord_client_cleanup(discord_client_t* client);

// Send notification via Discord
int discord_client_send(discord_client_t* client, const notification_event_t* event);

// Get discord client status
void discord_client_get_status(discord_client_t* client, discord_client_status_t* status);

// Create discord message from notification event
void discord_client_create_message(discord_client_t* client, const notification_event_t* event, 
                                  discord_message_t* message);

// Get embed color for notification priority
int discord_client_get_embed_color(notification_priority_t priority);

// Get priority text for display
const char* discord_client_get_priority_text(notification_priority_t priority);

#endif // DISCORD_CLIENT_H
