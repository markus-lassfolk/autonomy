#ifndef EMAIL_CLIENT_H
#define EMAIL_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Email client for sending email notifications

// Email configuration
typedef struct {
    char smtp_server[256];
    int smtp_port;
    char username[128];
    char password[128];
    char from_address[128];
    char from_name[128];
    bool use_tls;
    bool use_ssl;
    int timeout_seconds;
} email_config_t;

// Email message
typedef struct {
    char to[256];
    char cc[256];
    char bcc[256];
    char subject[256];
    char body[4096];
    char content_type[64];
    bool is_html;
} email_message_t;

// Function declarations
int email_client_init(const email_config_t *config);
void email_client_cleanup(void);
int email_client_send(const email_message_t *message);
int email_client_send_async(const email_message_t *message, void (*callback)(bool success, void *user_data), void *user_data);
int email_client_test_connection(void);

#endif // EMAIL_CLIENT_H
