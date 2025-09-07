#ifndef GPS_H
#define GPS_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS discovery and management functions

/**
 * Discover and initialize GPS sources
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int discover_gps_sources(void);

/**
 * Calculate GPS confidence score for a source
 * @param source GPS source to calculate confidence for
 * @return Confidence score (0-100)
 */
int calculate_gps_confidence(struct gps_source *source);

/**
 * Perform GPS health check and update all sources
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int perform_gps_health_check(void);

/**
 * Cleanup GPS system and stop all monitoring
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_H
