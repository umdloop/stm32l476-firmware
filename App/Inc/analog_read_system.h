#ifndef ANALOG_READ_SYSTEM_H
#define ANALOG_READ_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

bool analog_read_system_init(void);

void analog_read_system_controller(void);

#ifdef __cplusplus
}
#endif

#endif
