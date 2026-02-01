#include "main.h"

/* Platform */
#include "../../Platform/Inc/system_clock.h"
#include "../../Platform/Inc/gpio.h"
#include "../../Platform/Inc/can.h"
#include "../../Platform/Inc/usart.h"

/* __App__ */
#include "../../App/Inc/rr_scheduler.h"
#include "../../App/Inc/can_system.h"
#include "../../App/Inc/pcb_led_system.h"
// #include "../../__App__/Inc/ex_system.h"
#include "../../App/Inc/heartbeat_system.h"
#include "../../App/Inc/stepper_system.h"

#define STEPPER_MODEL_NEMA17 1

int main(void)
{
  HAL_Init();

  Platform_SystemClock_Config();

  MX_GPIO_Init();
  MX_CAN1_Init();
  MX_UART4_Init();

  RR_Scheduler_Init();
  RR_AddController(can_system_controller);
  RR_AddController(pcb_led_system_controller);
  RR_AddController(heartbeat_system_controller);
  RR_AddController(stepper_system_controller);
  // RR_AddController(ex_system_controller);

  // Simple delay to let system initialize
  HAL_Delay(100);

  // Spin motor 0 at 500 steps/second in forward direction
  // Change the port number (0-5) to test different motors
  // Change the speed (positive = forward, negative = reverse)
  StepperSystem_SetStepperModel(0, STEPPER_MODEL_NEMA17);  // Set motor model
  StepperSystem_SetSpeedStepsS(0, 500);  // 500 steps/second forward

  while (1)
  {
    RR_Scheduler_Tick();
  }
}
