#ifndef RR_SCHEDULER_H
#define RR_SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef void (*rr_controller_t)(void);

/* Clears the controller list */
void RR_Scheduler_Init(void);

/* Calls each registered controller once */
void RR_Scheduler_Tick(void);

/* Adds a controller if not already present. Returns true on success. */
bool RR_AddController(rr_controller_t controller);

/* Removes a controller if present. Returns true if removed. */
bool RR_RemoveController(rr_controller_t controller);

#ifdef __cplusplus
}
#endif

#endif /* RR_SCHEDULER_H */
