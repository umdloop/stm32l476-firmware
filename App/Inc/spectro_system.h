#ifndef SPECTRO_SYSTEM_H
#define SPECTRO_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

bool spectro_system_init(void);

/*
 * Main scheduler callback for this system.
 * Notes:
 * - This should be non-blocking.
 * - Avoid HAL_Delay() here.
 * - Use HAL_GetTick() for timed behavior.
 */
void spectro_system_controller(void);

#ifdef __cplusplus
}
#endif

#endif /* SPECTRO_SYSTEM_H */
