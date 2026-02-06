#include "main.h"
/* Platform */
#include "system_clock.h"
#include "gpio.h"
#include "can.h"
#include "usart.h"
#include "tim.h"
#include "can_system.h"
/* App */
#include "rr_scheduler.h"
#include "can_system.h"
#include "pcb_led_system.h"
#include "heartbeat_system.h"
#include "stepper_system.h"

int main(void)
{
  HAL_Init();
  Platform_SystemClock_Config();

  /* Initialize HW Peripherals */
  MX_GPIO_Init();
  MX_CAN1_Init();
  MX_UART4_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();

  /* Initialize App Systems */
  RR_Scheduler_Init();

  /* Register Controllers */

  RR_AddController(can_system_controller);
  RR_AddController(pcb_led_system_controller);
  RR_AddController(heartbeat_system_controller);
  RR_AddController(stepper_system_controller);

  /* Startup Blink */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
  HAL_Delay(500);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);

  while (1)
  {
    RR_Scheduler_Tick();
  }
}
