#ifndef CAN_H
#define CAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern CAN_HandleTypeDef hcan1;

void MX_CAN1_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_H */
