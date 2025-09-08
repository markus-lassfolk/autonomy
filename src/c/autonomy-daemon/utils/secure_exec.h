#ifndef SECURE_EXEC_H
#define SECURE_EXEC_H

#include "../core/types.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum command length for security
#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGUMENTS 32

// Command execution result
typedef struct {
    int exit_code;
    char output[4096];
    char error[1024];
    bool success;
} exec_result_t;

// Secure command execution using execve
int secure_exec_command(const char *command, exec_result_t *result);

// Secure command execution with arguments array
int secure_exec_args(char *const argv[], exec_result_t *result);

// Check if a command exists in PATH
bool command_exists(const char *command);

// Get full path of a command
int get_command_path(const char *command, char *full_path, size_t path_size);

// Validate command for security (whitelist approach)
bool is_command_allowed(const char *command);

// Safe UCI command execution
int secure_uci_command(const char *uci_args, exec_result_t *result);

// Safe systemctl command execution
int secure_systemctl_command(const char *action, const char *service, exec_result_t *result);

// Safe file operations
int secure_file_operation(const char *operation, const char *file_path, exec_result_t *result);

// Safe cellular device operations
int secure_cellular_at_command(const char *device_path, const char *at_command, exec_result_t *result);

// Check MWAN3 availability securely
int secure_check_mwan3_available(void);

#ifdef __cplusplus
}
#endif

#endif // SECURE_EXEC_H