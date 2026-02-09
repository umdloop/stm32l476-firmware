#ifndef CAN_SYSTEM_H
#define CAN_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* Round-robin scheduled controller.
 * Call this periodically from the main loop.
 */
void can_system_controller(void);

/* =========================
 * New public API
 * =========================
 *
 * CanSystem_Send() schedules exactly one transmit of a DBC-defined message.
 * It does NOT modify any CanParams values.
 *
 * Naming rules:
 *   - "MESSAGE"        : valid only for non-multiplexed messages
 *   - "MESSAGE.SIGNAL" : valid for non-mux and mux messages
 *
 * For mux messages, "MESSAGE.SIGNAL" schedules the mux page that SIGNAL
 * belongs to. (You guarantee there are no "always" signals in muxed messages.)
 *
 * Returns:
 *   - true  if the name maps to a schedulable message/page
 *   - false otherwise
 */
bool CanSystem_Send(const char* full_name);

/* Debug helpers (no counters)
 * - full_name follows the same naming rules as CanSystem_Send()
 */
bool CanSystem_DebugGetLastRxTick(const char* full_name, uint32_t* out_tick);
bool CanSystem_DebugGetLastTxTick(const char* full_name, uint32_t* out_tick);

/* =========================
 * Deprecated legacy API
 * =========================
 *
 * These perform set+send. Keep for compatibility, prefer:
 *   CanParams_Set*() then CanSystem_Send().
 */
#if defined(__GNUC__)
#define CAN_DEPRECATED __attribute__((deprecated))
#else
#define CAN_DEPRECATED
#endif

bool CAN_DEPRECATED CanSystem_SetBool(const char* full_name, bool value);
bool CAN_DEPRECATED CanSystem_SetInt32(const char* full_name, int32_t value);
bool CAN_DEPRECATED CanSystem_SetFloat(const char* full_name, float value);

#ifdef __cplusplus
}
#endif

#endif /* CAN_SYSTEM_H */
