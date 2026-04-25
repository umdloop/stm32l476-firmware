#ifndef DC_MOTOR_SYSTEM_H
#define DC_MOTOR_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

bool dc_motor_system_init(void);
void dc_motor_system_controller(void);

#ifdef __cplusplus
}
#endif

#endif /* DC_MOTOR_SYSTEM_H */
