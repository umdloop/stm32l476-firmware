#ifndef CAN_SYSTEM_H
#define CAN_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 2026 Protocol Frame Structure */
typedef struct {
  uint32_t can_id;
  uint8_t  len;
  uint8_t  data[8];
} CanFrame_t;

/* Function Prototypes */

void can_system_controller(void);

<<<<<<< HEAD
void CanSystem_Transmit(uint32_t arbitration_id, uint8_t* payload, uint8_t len);
/*
 * External TX-facing API
 *
 * Any system may call these to request that a CAN parameter be updated.
 * If the parameter corresponds to a DBC-defined signal, the CAN system will
 * mark the corresponding message (and mux page, if applicable) as dirty and
 * transmit it on its next round-robin tick.
 *
 * These functions will set the global CAN parameter "pending_outbox" to true.
 */
bool CanSystem_SetBool(const char* full_name, bool value);
bool CanSystem_SetInt32(const char* full_name, int32_t value);
bool CanSystem_SetFloat(const char* full_name, float value);
=======
void CanSystem_Transmit(uint32_t id, uint8_t* data, uint8_t len);
bool CanSystem_Receive(CanFrame_t* frame);
>>>>>>> 861de1f932ad4e7e6955641d40c3b4d457ce9318

#ifdef __cplusplus
}
#endif

#endif /* CAN_SYSTEM_H */
