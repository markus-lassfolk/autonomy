#include "autonomy_version.h"
#include <string.h>

// Static version info structure
static autonomy_version_info_t g_version_info = {
    .major = AUTONOMY_VERSION_MAJOR,
    .minor = AUTONOMY_VERSION_MINOR,
    .patch = AUTONOMY_VERSION_PATCH,
    .build = AUTONOMY_VERSION_BUILD,
    .version_string = AUTONOMY_VERSION,
    .full_version_string = AUTONOMY_VERSION_FULL,
    .git_hash = AUTONOMY_GIT_HASH,
    .git_branch = AUTONOMY_GIT_BRANCH,
    .build_date = AUTONOMY_BUILD_DATE
};

const autonomy_version_info_t* autonomy_get_version_info(void) {
    return &g_version_info;
}
