#ifndef API_SERVER_H
#define API_SERVER_H

#include "types.h"
#include "logx.h"

// Initialize API server with custom settings
int api_server_init(int port, const char *bind_address);

// Start API server with default settings
int api_server_start(void);

// Stop API server
void api_server_stop(void);

// Check if API server is running
bool api_server_is_running(void);

// Get API server port
int api_server_get_port(void);

// Get API server bind address
const char* api_server_get_bind_address(void);

// Set API server configuration
int api_server_set_config(int port, const char *bind_address);

// Cleanup API server
void api_server_cleanup(void);

#endif // API_SERVER_H
