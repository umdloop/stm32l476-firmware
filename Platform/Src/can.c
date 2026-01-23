#include "can.h"

CAN_HandleTypeDef hcan1;

void MX_CAN1_Init(void)
{
  __HAL_RCC_CAN1_CLK_ENABLE();

  hcan1.Instance = CAN1;

  /* Very conservative defaults. Timing must be set for your bus speed later. */
  hcan1.Init.Prescaler = 16;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_8TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_1TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = ENABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;

  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }

  /* We are not configuring filters or starting CAN here yet.
     That will belong in a CAN system later, not in main. */
}
