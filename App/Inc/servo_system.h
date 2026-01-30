#ifndef SERVO_SYSTEM_H
#define SERVO_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define SERVO_PORT_COUNT (8u)

/* Model IDs */
enum
{
  SERVO_MODEL_NONE     = 0,
  SERVO_MODEL_HS_645MG = 1,
};

/* Round-robin entrypoint */
void servo_system_controller(void);

/* Optional direct name */
void ServoSystem_Controller(void);

bool ServoSystem_SetServoModel(uint8_t port, uint8_t model_id);
uint8_t ServoSystem_GetServoModel(uint8_t port);

bool ServoSystem_SetPositionDeg(uint8_t port, float position_deg);
bool ServoSystem_SetVelocityDegS(uint8_t port, float velocity_deg_s);

void ServoSystem_OnSetZero(uint8_t port);
void ServoSystem_OnRequestVectors(uint8_t port);
void ServoSystem_OnStopMotor(uint8_t port);
void ServoSystem_OnShutdownMotor(uint8_t port);
void ServoSystem_OnClearErrors(uint8_t port);

#ifdef __cplusplus
}
#endif

#endif /* SERVO_SYSTEM_H */
