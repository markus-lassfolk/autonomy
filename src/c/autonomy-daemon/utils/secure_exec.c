#include "secure_exec.h"
#include "../utils/logx.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

// Allowed commands whitelist for security
static const char* ALLOWED_COMMANDS[] = {
    "uci", "systemctl", "ubus", "gsmctl", "microcom", "timeout",
    "grep", "awk", "sed", "cut", "head", "tail", "sort", "uniq",
    "find", "ls", "cat", "echo", "printf", "test", "which",
    "ip", "route", "ifconfig", "ping", "wget", "curl",
    "sqlite3", "opkg", "fsck", "ntpdate", "sync",
    "wifi", "hostapd_cli", "iwconfig", "iwlist", "iw", "ethtool",
    NULL
};

// Allowed UCI operations
static const char* ALLOWED_UCI_OPERATIONS[] = {
    "show", "get", "set", "delete", "add", "commit", "revert",
    "changes", "batch", "export", "import", "rename", "reorder",
    NULL
};

// Allowed systemctl operations
static const char* ALLOWED_SYSTEMCTL_OPERATIONS[] = {
    "start", "stop", "restart", "reload", "status", "is-active",
    "is-enabled", "enable", "disable", "mask", "unmask",
    NULL
};

// Initialize exec result
static void init_exec_result(exec_result_t *result) {
    if (!result) return;
    
    result->exit_code = -1;
    result->output[0] = '\0';
    result->error[0] = '\0';
    result->success = false;
}

// Check if command is in whitelist
bool is_command_allowed(const char *command) {
    if (!command) return false;
    
    // Extract the first word (command name)
    char cmd_name[256];
    sscanf(command, "%255s", cmd_name);
    
    for (int i = 0; ALLOWED_COMMANDS[i] != NULL; i++) {
        if (strcmp(cmd_name, ALLOWED_COMMANDS[i]) == 0) {
            return true;
        }
    }
    
    return false;
}

// Check if UCI operation is allowed
static bool is_uci_operation_allowed(const char *operation) {
    if (!operation) return false;
    
    for (int i = 0; ALLOWED_UCI_OPERATIONS[i] != NULL; i++) {
        if (strcmp(operation, ALLOWED_UCI_OPERATIONS[i]) == 0) {
            return true;
        }
    }
    
    return false;
}

// Check if systemctl operation is allowed
static bool is_systemctl_operation_allowed(const char *operation) {
    if (!operation) return false;
    
    for (int i = 0; ALLOWED_SYSTEMCTL_OPERATIONS[i] != NULL; i++) {
        if (strcmp(operation, ALLOWED_SYSTEMCTL_OPERATIONS[i]) == 0) {
            return true;
        }
    }
    
    return false;
}

// Parse command into arguments array
static int parse_command_args(const char *command, char *args[], int max_args) {
    if (!command || !args || max_args <= 0) return 0;
    
    int argc = 0;
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return 0;
    
    char *token = strtok(cmd_copy, " \t\n");
    while (token && argc < max_args - 1) {
        args[argc] = strdup(token);
        argc++;
        token = strtok(NULL, " \t\n");
    }
    args[argc] = NULL;
    
    free(cmd_copy);
    return argc;
}

// Secure command execution using execve
int secure_exec_command(const char *command, exec_result_t *result) {
    if (!command || !result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    init_exec_result(result);
    
    // Check command length
    if (strlen(command) > MAX_COMMAND_LENGTH) {
        snprintf(result->error, sizeof(result->error), "Command too long");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Check if command is allowed
    if (!is_command_allowed(command)) {
        snprintf(result->error, sizeof(result->error), "Command not allowed: %s", command);
        LOGX_WARN("Blocked unauthorized command: %s", command);
        return AUTONOMY_ERROR_SECURITY;
    }
    
    // Parse command into arguments
    char *args[MAX_ARGUMENTS];
    int argc = parse_command_args(command, args, MAX_ARGUMENTS);
    if (argc == 0) {
        snprintf(result->error, sizeof(result->error), "Failed to parse command");
        return AUTONOMY_ERROR_PARSE_FAILED;
    }
    
    // Execute with arguments array
    int ret = secure_exec_args(args, result);
    
    // Clean up allocated arguments
    for (int i = 0; i < argc; i++) {
        free(args[i]);
    }
    
    return ret;
}

// Secure command execution with arguments array
int secure_exec_args(char *const argv[], exec_result_t *result) {
    if (!argv || !result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    init_exec_result(result);
    
    if (!argv[0]) {
        snprintf(result->error, sizeof(result->error), "No command specified");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Check if command is allowed
    if (!is_command_allowed(argv[0])) {
        snprintf(result->error, sizeof(result->error), "Command not allowed: %s", argv[0]);
        LOGX_WARN("Blocked unauthorized command: %s", argv[0]);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Create pipes for output capture
    int stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1) {
        snprintf(result->error, sizeof(result->error), "Failed to create pipes");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        snprintf(result->error, sizeof(result->error), "Failed to fork process");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    if (pid == 0) {
        // Child process
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        
        // Redirect stdout and stderr to pipes
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        
        // Execute command
        execvp(argv[0], argv);
        
        // If we get here, execvp failed
        _exit(127);
    } else {
        // Parent process
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        
        // Read output
        ssize_t bytes_read = read(stdout_pipe[0], result->output, sizeof(result->output) - 1);
        if (bytes_read > 0) {
            result->output[bytes_read] = '\0';
        }
        
        // Read error
        bytes_read = read(stderr_pipe[0], result->error, sizeof(result->error) - 1);
        if (bytes_read > 0) {
            result->error[bytes_read] = '\0';
        }
        
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        
        // Wait for child process
        int status;
        waitpid(pid, &status, 0);
        
        result->exit_code = WEXITSTATUS(status);
        result->success = (result->exit_code == 0);
        
        return AUTONOMY_SUCCESS;
    }
}

// Check if a command exists in PATH
bool command_exists(const char *command) {
    if (!command) return false;
    
    char *path = getenv("PATH");
    if (!path) return false;
    
    char *path_copy = strdup(path);
    if (!path_copy) return false;
    
    char *dir = strtok(path_copy, ":");
    while (dir) {
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, command);
        
        struct stat st;
        if (stat(full_path, &st) == 0 && (st.st_mode & S_IXUSR)) {
            free(path_copy);
            return true;
        }
        
        dir = strtok(NULL, ":");
    }
    
    free(path_copy);
    return false;
}

// Get full path of a command
int get_command_path(const char *command, char *full_path, size_t path_size) {
    if (!command || !full_path || path_size == 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    char *path = getenv("PATH");
    if (!path) {
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    char *path_copy = strdup(path);
    if (!path_copy) {
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    char *dir = strtok(path_copy, ":");
    while (dir) {
        snprintf(full_path, path_size, "%s/%s", dir, command);
        
        struct stat st;
        if (stat(full_path, &st) == 0 && (st.st_mode & S_IXUSR)) {
            free(path_copy);
            return AUTONOMY_SUCCESS;
        }
        
        dir = strtok(NULL, ":");
    }
    
    free(path_copy);
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Safe UCI command execution
int secure_uci_command(const char *uci_args, exec_result_t *result) {
    if (!uci_args || !result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Parse UCI operation
    char operation[64];
    if (sscanf(uci_args, "%63s", operation) != 1) {
        snprintf(result->error, sizeof(result->error), "Invalid UCI command format");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Check if UCI operation is allowed
    if (!is_uci_operation_allowed(operation)) {
        snprintf(result->error, sizeof(result->error), "UCI operation not allowed: %s", operation);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Build command
    char command[MAX_COMMAND_LENGTH];
    snprintf(command, sizeof(command), "uci %s", uci_args);
    
    return secure_exec_command(command, result);
}

// Safe systemctl command execution
int secure_systemctl_command(const char *action, const char *service, exec_result_t *result) {
    if (!action || !service || !result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Check if systemctl operation is allowed
    if (!is_systemctl_operation_allowed(action)) {
        snprintf(result->error, sizeof(result->error), "systemctl operation not allowed: %s", action);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Build command
    char command[MAX_COMMAND_LENGTH];
    snprintf(command, sizeof(command), "systemctl %s %s", action, service);
    
    return secure_exec_command(command, result);
}

// Safe file operations
int secure_file_operation(const char *operation, const char *file_path, exec_result_t *result) {
    if (!operation || !file_path || !result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate file path (basic security check)
    if (strstr(file_path, "..") || strstr(file_path, "~")) {
        snprintf(result->error, sizeof(result->error), "File path not allowed: %s", file_path);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Only allow operations in safe directories
    if (!strstr(file_path, "/var/lib/autonomy/") && 
        !strstr(file_path, "/tmp/autonomy/") && 
        !strstr(file_path, "/var/log/autonomy/")) {
        snprintf(result->error, sizeof(result->error), "File path not in allowed directory: %s", file_path);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Build command based on operation
    char command[MAX_COMMAND_LENGTH];
    if (strcmp(operation, "remove") == 0) {
        snprintf(command, sizeof(command), "rm -f %s", file_path);
    } else if (strcmp(operation, "create") == 0) {
        snprintf(command, sizeof(command), "touch %s", file_path);
    } else {
        snprintf(result->error, sizeof(result->error), "File operation not supported: %s", operation);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    return secure_exec_command(command, result);
}

// Safe cellular device AT command execution
int secure_cellular_at_command(const char *device_path, const char *at_command, exec_result_t *result) {
    if (!device_path || !at_command || !result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate device path
    if (!strstr(device_path, "/dev/tty")) {
        snprintf(result->error, sizeof(result->error), "Invalid device path: %s", device_path);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Check if device exists and is accessible
    if (access(device_path, R_OK | W_OK) != 0) {
        snprintf(result->error, sizeof(result->error), "Device not accessible: %s", device_path);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Validate AT command
    if (strlen(at_command) > 32 || strstr(at_command, ";") || strstr(at_command, "&")) {
        snprintf(result->error, sizeof(result->error), "Invalid AT command: %s", at_command);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Build secure command
    char command[MAX_COMMAND_LENGTH];
    snprintf(command, sizeof(command), "timeout 2 sh -c 'echo \"%s\" > %s && head -1 < %s'", 
             at_command, device_path, device_path);
    
    return secure_exec_command(command, result);
}

// Check MWAN3 availability securely
int secure_check_mwan3_available(void) {
    exec_result_t result;
    
    // First check if UCI is available
    int ret = secure_exec_command("which uci", &result);
    if (ret != AUTONOMY_SUCCESS || !result.success) {
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Then check if MWAN3 config exists
    ret = secure_uci_command("show mwan3", &result);
    if (ret != AUTONOMY_SUCCESS || !result.success) {
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    return AUTONOMY_SUCCESS;
}
