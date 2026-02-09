#ifndef CAN_PARAMS_H
#define CAN_PARAMS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/*
 * CAN parameter database
 *
 * This module is a pure in-memory database. It does NOT schedule or transmit
 * CAN messages.
 *
 * Names:
 *   - Signal parameters: "MESSAGE.SIGNAL"
 *   - Message names:     "MESSAGE" (only meaningful for Event parameters on
 *                       non-multiplexed messages)
 *
 * Types:
 *   - Bool / Int32 / Float : normal value parameters
 *   - Event                : latched flag set by the CAN RX path
 *
 * Event semantics:
 *   - CanParams_GetEvent() reads the latched flag without clearing it.
 *   - CanParams_ProcEvent() reads and clears ONLY if it was true.
 *
 * Reserved legacy globals (DEPRECATED):
 *   - pending_inbox
 *   - pending_outbox
 */

/* -------------------------
 * Value access
 * ------------------------- */
bool CanParams_GetBool(const char* full_name, bool* out_value);
bool CanParams_GetInt32(const char* full_name, int32_t* out_value);
bool CanParams_GetFloat(const char* full_name, float* out_value);

bool CanParams_SetBool(const char* full_name, bool value);
bool CanParams_SetInt32(const char* full_name, int32_t value);
bool CanParams_SetFloat(const char* full_name, float value);

/* =========================
 * Deprecated legacy API
 * =========================
 *
 * Older code used CanParams_IsValid() to gate behavior until a parameter was
 * received at least once. Parameters are now initialized to defaults at startup,
 * so this is effectively "does the parameter exist".
 */
#if defined(__GNUC__)
#define CAN_DEPRECATED __attribute__((deprecated))
#else
#define CAN_DEPRECATED
#endif

bool CAN_DEPRECATED CanParams_IsValid(const char* full_name);

/* -------------------------
 * Event access
 * ------------------------- */
bool CanParams_GetEvent(const char* full_name, bool* out_event);
bool CanParams_ProcEvent(const char* full_name, bool* out_event);
bool CanParams_SetEvent(const char* full_name, bool event_value);

/* =========================
 * Internal-only helpers used by CAN system
 * ========================= */

typedef enum
{
  CANP_TYPE_BOOL = 0,
  CANP_TYPE_INT32,
  CANP_TYPE_FLOAT,
  CANP_TYPE_EVENT
} canp_type_t;

void CanParams__Reset(void);
bool CanParams__Create(const char* full_name, canp_type_t type);

/* Link a non-event parameter to an event parameter.
 * - param_full_name may be any existing parameter (Bool/Int32/Float).
 * - event_full_name must refer to an existing CANP_TYPE_EVENT parameter.
 */
bool CanParams__LinkEvent(const char* param_full_name, const char* event_full_name);

/* RX-update entrypoints (decoded from CAN frames). These do NOT schedule TX. */
bool CanParams__UpdateBool(const char* full_name, uint8_t value);
bool CanParams__UpdateInt32(const char* full_name, int32_t value);
bool CanParams__UpdateFloat(const char* full_name, float value);
bool CanParams__UpdateEvent(const char* full_name, bool event_value);

#ifdef __cplusplus
}
#endif

#endif /* CAN_PARAMS_H */
