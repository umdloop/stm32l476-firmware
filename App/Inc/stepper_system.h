#ifndef STEPPER_SYSTEM_H
#define STEPPER_SYSTEM_H

#include <stdint.h>

#define STEPPER_PORT_COUNT 6

/* Public API for the Scheduler */
void stepper_system_controller(void);

#endif
