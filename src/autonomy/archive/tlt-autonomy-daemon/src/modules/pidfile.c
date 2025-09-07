#include "autonomy_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>

#define PID_FILE "/var/run/autonomy-daemon.pid"

// PID file management
int create_pid_file(void) {
    FILE *pid_file = fopen(PID_FILE, "w");
    if (!pid_file) {
        syslog(LOG_ERR, "Failed to create PID file: %s", strerror(errno));
        return -1;
    }
    fprintf(pid_file, "%d\n", getpid());
    fclose(pid_file);
    return 0;
}

void remove_pid_file(void) {
    unlink(PID_FILE);
}

int check_pid_file(void) {
    FILE *pid_file = fopen(PID_FILE, "r");
    if (!pid_file) {
        return 0; // No existing PID file
    }
    
    int existing_pid;
    if (fscanf(pid_file, "%d", &existing_pid) == 1) {
        fclose(pid_file);
        
        // Check if process is actually running
        if (kill(existing_pid, 0) == 0) {
            syslog(LOG_ERR, "Autonomy daemon already running with PID %d", existing_pid);
            return -1;
        }
    }
    fclose(pid_file);
    return 0;
}
