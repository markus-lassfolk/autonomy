#ifndef VERSION_H
#define VERSION_H

// Version information for autonomy-daemon
#define AUTONOMY_DAEMON_VERSION_MAJOR 5
#define AUTONOMY_DAEMON_VERSION_MINOR 8
#define AUTONOMY_DAEMON_VERSION_PATCH 4
#define AUTONOMY_DAEMON_VERSION_BUILD 239

// Version strings
#define AUTONOMY_DAEMON_VERSION "5.8.4"
#define AUTONOMY_DAEMON_VERSION_FULL "5.8.4-239"

// Build information
#define AUTONOMY_DAEMON_BUILD_DATE __DATE__
#define AUTONOMY_DAEMON_BUILD_TIME __TIME__

// Git information (if available)
#ifndef AUTONOMY_DAEMON_GIT_COMMIT
#define AUTONOMY_DAEMON_GIT_COMMIT "unknown"
#endif

#ifndef AUTONOMY_DAEMON_GIT_BRANCH
#define AUTONOMY_DAEMON_GIT_BRANCH "unknown"
#endif

// Function to get formatted version string
const char* autonomy_daemon_get_version_string(void);
const char* autonomy_daemon_get_build_info_string(void);

#endif // VERSION_H
