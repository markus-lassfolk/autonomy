#ifndef AUTONOMY_VERSION_H
#define AUTONOMY_VERSION_H

/*
 * Centralized version management for Autonomy project
 * 
 * This file contains all version information for the Autonomy project.
 * Update the version numbers here and they will be automatically
 * synchronized across all components.
 */

// Main version information
#define AUTONOMY_VERSION_MAJOR    5
#define AUTONOMY_VERSION_MINOR    7
#define AUTONOMY_VERSION_PATCH    0
#define AUTONOMY_VERSION_BUILD     1

// Version string macros
#define AUTONOMY_VERSION_STRINGIFY(x) #x
#define AUTONOMY_VERSION_CONCAT(a, b, c) AUTONOMY_VERSION_STRINGIFY(a) "." AUTONOMY_VERSION_STRINGIFY(b) "." AUTONOMY_VERSION_STRINGIFY(c)

// Full version string (e.g., "1.0.0")
#define AUTONOMY_VERSION AUTONOMY_VERSION_CONCAT(AUTONOMY_VERSION_MAJOR, AUTONOMY_VERSION_MINOR, AUTONOMY_VERSION_PATCH)

// Full version with build (e.g., "1.0.0-1")
#define AUTONOMY_VERSION_FULL AUTONOMY_VERSION "-" AUTONOMY_VERSION_STRINGIFY(AUTONOMY_VERSION_BUILD)

// Component-specific versions (can be overridden if needed)
#define AUTONOMY_DAEMON_VERSION AUTONOMY_VERSION_FULL
#define AUTONOMY_API_VERSION AUTONOMY_VERSION_FULL
#define AUTONOMY_UI_VERSION AUTONOMY_VERSION_FULL
#define STARLINK_TRACKING_VERSION AUTONOMY_VERSION_FULL

// Version information for build system
#define AUTONOMY_PKG_VERSION AUTONOMY_VERSION
#define AUTONOMY_PKG_RELEASE AUTONOMY_VERSION_BUILD

// Git information (can be set by build system)
#ifndef AUTONOMY_GIT_HASH
#define AUTONOMY_GIT_HASH "unknown"
#endif

#ifndef AUTONOMY_GIT_BRANCH
#define AUTONOMY_GIT_BRANCH "main"
#endif

#ifndef AUTONOMY_BUILD_DATE
#define AUTONOMY_BUILD_DATE __DATE__ " " __TIME__
#endif

// Version info structure for runtime access
typedef struct {
    int major;
    int minor;
    int patch;
    int build;
    const char *version_string;
    const char *full_version_string;
    const char *git_hash;
    const char *git_branch;
    const char *build_date;
} autonomy_version_info_t;

// Function to get version information
const autonomy_version_info_t* autonomy_get_version_info(void);

// Convenience macros for common version checks
#define AUTONOMY_VERSION_AT_LEAST(major, minor, patch) \
    (AUTONOMY_VERSION_MAJOR > (major) || \
     (AUTONOMY_VERSION_MAJOR == (major) && AUTONOMY_VERSION_MINOR > (minor)) || \
     (AUTONOMY_VERSION_MAJOR == (major) && AUTONOMY_VERSION_MINOR == (minor) && AUTONOMY_VERSION_PATCH >= (patch)))

#define AUTONOMY_VERSION_EQUALS(major, minor, patch) \
    (AUTONOMY_VERSION_MAJOR == (major) && AUTONOMY_VERSION_MINOR == (minor) && AUTONOMY_VERSION_PATCH == (patch))

#endif // AUTONOMY_VERSION_H
