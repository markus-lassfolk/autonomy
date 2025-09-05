#ifndef DISCORD_CLIENT_H
#define DISCORD_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Discord client for sending Discord webhook notifications

// Discord configuration
typedef struct {
    char webhook_url[512];
    char username[64];
    char avatar_url[512];
    bool tts;
    int timeout_seconds;
} discord_config_t;

// Discord message
typedef struct {
    char content[2000];
    char username[64];
    char avatar_url[512];
    bool tts;
    char embeds[4096]; // JSON string for embeds
} discord_message_t;

// Function declarations
int discord_client_init(const discord_config_t *config);
void discord_client_cleanup(void);
int discord_client_send(const discord_message_t *message);
int discord_client_send_async(const discord_message_t *message, void (*callback)(bool success, void *user_data), void *user_data);
int discord_client_test_connection(void);

#endif // DISCORD_CLIENT_H
