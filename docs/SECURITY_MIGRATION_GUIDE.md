# Security Migration Guide

## Overview

This document provides guidance for migrating from insecure `system()` calls to secure alternatives using the new `secure_exec` utility and shared constants.

## Security Issues with system() Calls

### Problems

1. **Command Injection**: Vulnerable to shell injection attacks
2. **Path Traversal**: Can execute arbitrary commands
3. **Environment Pollution**: Inherits all environment variables
4. **No Input Validation**: No validation of command arguments
5. **Shell Interpretation**: Commands are interpreted by shell

### Example Vulnerable Code

```c
// VULNERABLE: Command injection possible
char user_input[256];
scanf("%s", user_input);
char command[512];
snprintf(command, sizeof(command), "ls %s", user_input);
system(command); // Dangerous!

// VULNERABLE: Path traversal possible
char filename[256];
scanf("%s", filename);
char command[512];
snprintf(command, sizeof(command), "cat %s", filename);
system(command); // Dangerous!
```

## Secure Alternatives

### 1. Using secure_exec Utility

#### Before (Insecure)

```c
// OLD: Insecure system() call
int ret = system("uci show mwan3 > /dev/null 2>&1");
if (ret == 0) {
    // MWAN3 is available
}
```

#### After (Secure)

```c
// NEW: Secure execution
#include "utils/secure_exec.h"

exec_result_t result;
int ret = secure_uci_command("show mwan3", &result);
if (ret == AUTONOMY_SUCCESS && result.success) {
    // MWAN3 is available
    LOGX_DEBUG_MSG("MWAN3 output: %s", result.output);
}
```

### 2. Using Shared Constants

#### Before (Hardcoded)

```c
// OLD: Hardcoded duration windows
const char* duration_windows[] = {
    "<2sec", "2-5sec", "5-10sec", "10-30sec", "30-60sec", 
    "1-2min", "2-5min", "5-15min", "15-60min", "1-4hours", ">4hours"
};
```

#### After (Shared Constants)

```c
// NEW: Using shared constants
#include "core/constants.h"

// Use predefined constants
for (int i = 0; i < DURATION_WINDOW_COUNT; i++) {
    printf("Window %d: %s\n", i, DURATION_WINDOWS[i]);
}

// Get duration window for specific time
int seconds = 45;
const char* window = get_duration_window_label(seconds);
printf("45 seconds falls in: %s\n", window);
```

## Migration Examples

### 1. UCI Commands

#### Before

```c
char uci_cmd[256];
snprintf(uci_cmd, sizeof(uci_cmd), "uci set network.wan.proto=dhcp");
int ret = system(uci_cmd);
if (ret == 0) {
    system("uci commit network");
}
```

#### After

```c
#include "utils/secure_exec.h"

exec_result_t result;
int ret = secure_uci_command("set network.wan.proto=dhcp", &result);
if (ret == AUTONOMY_SUCCESS && result.success) {
    secure_uci_command("commit network", &result);
}
```

### 2. Systemctl Commands

#### Before (Systemctl)

```c
char cmd[256];
snprintf(cmd, sizeof(cmd), "systemctl restart %s", service_name);
int ret = system(cmd);
```

#### After (Systemctl)

```c
#include "utils/secure_exec.h"

exec_result_t result;
int ret = secure_systemctl_command("restart", service_name, &result);
if (ret == AUTONOMY_SUCCESS && result.success) {
    LOGX_INFO_MSG("Service %s restarted successfully", service_name);
}
```

### 3. File Operations

#### Before (File Operations)

```c
char cmd[256];
snprintf(cmd, sizeof(cmd), "rm -f %s", filename);
system(cmd);
```

#### After (File Operations)

```c
#include "utils/secure_exec.h"

exec_result_t result;
int ret = secure_file_operation("remove", filename, &result);
if (ret == AUTONOMY_SUCCESS && result.success) {
    LOGX_DEBUG_MSG("File %s removed successfully", filename);
}
```

### 4. Complex Commands

#### Before (Complex Commands)

```c
char command[512];
snprintf(command, sizeof(command), 
         "echo 'AT+CSQ' | timeout 2 microcom -t 1000 %s 2>/dev/null | grep -q 'OK'", 
         device_path);
int ret = system(command);
```

#### After (Complex Commands)

```c
#include "utils/secure_exec.h"

// Split into multiple secure operations
exec_result_t result;

// First, check if device exists and is accessible
if (access(device_path, R_OK | W_OK) != 0) {
    return AUTONOMY_ERROR_ACCESS_DENIED;
}

// Use the cellular device helper instead
#include "network/cellular_device_helper.h"
int rssi, ber;
int ret = get_signal_strength_dynamic(&rssi, &ber);
```

## Security Features

### 1. Command Whitelist

Only pre-approved commands are allowed:

- `uci`, `systemctl`, `ubus`, `gsmctl`, `microcom`
- `grep`, `awk`, `sed`, `cut`, `head`, `tail`
- `find`, `ls`, `cat`, `echo`, `printf`
- `ip`, `route`, `ifconfig`, `ping`
- `sqlite3`, `opkg`, `fsck`, `ntpdate`

### 2. Operation Validation

- UCI operations: `show`, `get`, `set`, `delete`, `add`, `commit`
- systemctl operations: `start`, `stop`, `restart`, `reload`, `status`
- File operations: `remove`, `create` (with path validation)

### 3. Input Sanitization

- Command length limits
- Path traversal protection
- Argument validation
- Environment isolation

### 4. Error Handling

- Comprehensive error reporting
- Security violation logging
- Graceful failure handling

## Best Practices

### 1. Always Use Secure Alternatives

```c
// GOOD: Use secure_exec
exec_result_t result;
int ret = secure_exec_command("allowed_command arg1 arg2", &result);

// BAD: Never use system() directly
int ret = system("any_command");
```

### 2. Validate Inputs

```c
// GOOD: Validate inputs before use
if (!is_command_allowed(command)) {
    LOGX_ERROR_MSG("Command not allowed: %s", command);
    return AUTONOMY_ERROR_SECURITY;
}

// BAD: Use inputs without validation
system(user_input);
```

### 3. Use Shared Constants

```c
// GOOD: Use shared constants
const char* window = get_duration_window_label(seconds);

// BAD: Hardcode values
const char* window = "<2sec"; // Hardcoded
```

### 4. Handle Errors Properly

```c
// GOOD: Check all return values
exec_result_t result;
int ret = secure_exec_command(command, &result);
if (ret != AUTONOMY_SUCCESS) {
    LOGX_ERROR_MSG("Command execution failed: %s", result.error);
    return ret;
}

// BAD: Ignore return values
system(command); // No error checking
```

## Migration Checklist

- [ ] Identify all `system()` calls in the codebase
- [ ] Replace with appropriate `secure_exec_*` functions
- [ ] Replace hardcoded constants with shared constants
- [ ] Add proper error handling
- [ ] Test all migrated functionality
- [ ] Update documentation
- [ ] Review security implications

## Testing

### Unit Tests

```c
#include "utils/secure_exec.h"

void test_secure_exec() {
    exec_result_t result;
    
    // Test allowed command
    int ret = secure_exec_command("echo test", &result);
    assert(ret == AUTONOMY_SUCCESS);
    assert(result.success);
    assert(strstr(result.output, "test") != NULL);
    
    // Test blocked command
    ret = secure_exec_command("rm -rf /", &result);
    assert(ret == AUTONOMY_ERROR_SECURITY);
    assert(!result.success);
}
```

### Integration Tests

- Test all migrated system() calls
- Verify security restrictions work
- Test error handling paths
- Validate output parsing

## Performance Considerations

### Advantages

- Better error handling
- More secure execution
- Consistent behavior
- Easier debugging

### Considerations

- Slightly more overhead due to fork/exec
- More memory usage for result structures
- Additional validation overhead

## Future Enhancements

1. **Process Isolation**: Use containers or namespaces
2. **Resource Limits**: Set CPU/memory limits
3. **Audit Logging**: Log all command executions
4. **Dynamic Whitelist**: Runtime command approval
5. **Sandboxing**: Further isolation of executed commands
