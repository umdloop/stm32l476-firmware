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
  SERVO_MODEL_DFROBOT = 2,
  SERVO_MODEL_GOBILDA_5TURN = 3,
  SERVO_MODEL_HS_5055MG = 4,
  SERVO_MODEL_FT_6335M = 5,
  SERVO_MODEL_MG_90S = 6,
};

/* Model Type */
enum
{
  SERVO_TYPE_UNDEFINED = 0,
  SERVO_TYPE_STANDARD = 1,
  SERVO_TYPE_CONTINUOUS = 2,
};

/* Motor Status*/
enum
{
  MOTOR_STATUS_UNDEFINED = 0,
  MOTOR_STATUS_IDLE = 1,
  MOTOR_STATUS_STARTUP = 2,
  MOTOR_STATUS_ERROR_INVALID_REQUEST = 3,
  MOTOR_STATUS_ERROR_DISARMED = 4,
  MOTOR_STATUS_ERROR_MOTOR_FAILED = 5,
  MOTOR_STATUS_ERROR_CONTROLLER_FAILED = 6,
  MOTOR_STATUS_ERROR_ESTOP = 7,
  MOTOR_STATUS_ERROR_UNKNOWN_POSITION = 8,
  MOTOR_STATUS_POSITION_CONTROL = 9,
  MOTOR_STATUS_VELOCITY_CONTROL = 10,
  MOTOR_STATUS_STOPPED = 11,
};

/* Round-robin entrypoint */
void servo_system_controller(void);

/* Optional direct name */
void ServoSystem_Controller(void);

bool ServoSystem_SetServoModel(uint8_t port, uint8_t model_id);
uint8_t ServoSystem_GetServoType(uint8_t port);
uint8_t ServoSystem_GetServoModel(uint8_t port);
uint16_t ServoSystem_GetPosMax(uint8_t port);
uint16_t ServoSystem_GetVelMax(uint8_t port);

bool ServoSystem_SetPositionDeg(uint8_t port, float position_deg);
bool ServoSystem_SetVelocityDegS(uint8_t port, float velocity_deg_s);

void ServoSystem_OnMotorStatusCmd(uint8_t port);
void ServoSystem_OnMotorSpecCmd(uint8_t port);

void ServoSystem_OnSetZero(uint8_t port);
void ServoSystem_OnRequestVectors(uint8_t port);
void ServoSystem_OnStopMotor(uint8_t port);
void ServoSystem_OnShutdownMotor(uint8_t port);
void ServoSystem_OnClearErrors(uint8_t port);

#ifdef __cplusplus
}
#endif

#endif /* SERVO_SYSTEM_H */
