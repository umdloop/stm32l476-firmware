#ifndef COPY_RENAME_ME_SYSTEM_H
#define COPY_RENAME_ME_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/*
 * Rename this file pair and rename the function prefixes below.
 *
 * Recommended pattern:
 *   my_feature_system.h
 *   my_feature_system.c
 *
 *   bool my_feature_system_init(void);
 *   void my_feature_system_controller(void);
 */

/*
 * Initializes any one-time state for the system.
 *
 * Returns:
 *   true  -> init succeeded
 *   false -> init failed
 *
 * Notes:
 * - Safe to call once at startup.
 * - You can also lazy-init from the controller if you prefer.
 */
bool copy_rename_me_system_init(void);

/*
 * Main scheduler callback for this system.
 *
 * Add this to main.c with:
 *   RR_AddController(copy_rename_me_system_controller);
 *
 * Notes:
 * - This should be non-blocking.
 * - Avoid HAL_Delay() here.
 * - Use HAL_GetTick() for timed behavior.
 */
void copy_rename_me_system_controller(void);

#ifdef __cplusplus
}
#endif

#endif /* COPY_RENAME_ME_SYSTEM_H */
