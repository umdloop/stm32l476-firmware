#ifndef CAN_PARAMS_H
#define CAN_PARAMS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/*
 * CAN parameter store
 *
 * Parameters are addressed by full signal name: "MESSAGE.SIGNAL"
 * (e.g. "SERVO_PCB_C.led_status")
 *
 * The CAN system creates all DBC-defined parameters in memory at startup,
 * plus two global flags:
 *   - pending_inbox
 *   - pending_outbox
 */

/* Public API */
bool CanParams_IsValid(const char* full_name);

bool CanParams_GetBool(const char* full_name, bool* out_value);
bool CanParams_GetInt32(const char* full_name, int32_t* out_value);
bool CanParams_GetFloat(const char* full_name, float* out_value);

bool CanParams_SetBool(const char* full_name, bool value);
bool CanParams_SetInt32(const char* full_name, int32_t value);
bool CanParams_SetFloat(const char* full_name, float value);

/*
 * Internal-only helpers used by the CAN system:
 * - Create/reset the parameter table
 * - Update* functions do NOT mark pending_outbox / TX-dirty;
 *   they are used for RX updates to avoid echoing messages back out.
 */
typedef enum
{
  CANP_TYPE_BOOL = 0,
  CANP_TYPE_INT32,
  CANP_TYPE_FLOAT
} canp_type_t;

void CanParams__Reset(void);
bool CanParams__Create(const char* full_name, canp_type_t type);

bool CanParams__UpdateBool(const char* full_name, uint8_t value);
bool CanParams__UpdateInt32(const char* full_name, int32_t value);
bool CanParams__UpdateFloat(const char* full_name, float value);

#ifdef __cplusplus
}
#endif

#endif /* CAN_PARAMS_H */
