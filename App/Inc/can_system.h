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

void CanSystem_Transmit(uint32_t id, uint8_t* data, uint8_t len);
bool CanSystem_Receive(CanFrame_t* frame);

#ifdef __cplusplus
}
#endif

#endif /* CAN_SYSTEM_H */
