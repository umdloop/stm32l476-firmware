#include "main.h"

/* Platform */
#include "../../Platform/Inc/system_clock.h"
#include "../../Platform/Inc/gpio.h"
#include "../../Platform/Inc/can.h"
#include "../../Platform/Inc/usart.h"

/* App */
#include "../../App/Inc/rr_scheduler.h"
#include "../../App/Inc/can_system.h"
#include "../../App/Inc/pcb_led_system.h"
#include "../../App/Inc/ex_system.h"
#include "../../App/Inc/heartbeat_system.h"
#include "../../App/Inc/servo_system.h"
#include "../../App/Inc/dbc_examples_system.h"
#include "../../App/Inc/test_pwm_system.h"

// Basic can testing (can raw)
#include "can_system.h"

int main(void)
{
  HAL_Init();

  Platform_SystemClock_Config();

  MX_GPIO_Init();
  MX_CAN1_Init();
  MX_UART4_Init();

  RR_Scheduler_Init();
  RR_AddController(can_system_controller);

  /* Core application systems */
  RR_AddController(pcb_led_system_controller);
  RR_AddController(heartbeat_system_controller);
  //RR_AddController(servo_system_controller);

  /* Optional example/demo systems. Uncomment when you want to exercise the
   * CAN API or canned DBC response flows from a bus analyzer.
   */
  // RR_AddController(ex_system_controller);
  // RR_AddController(dbc_examples_system_controller);
  // RR_AddController(test_pwm_system_controller);
  while (1)
  {
    RR_Scheduler_Tick();
    // HAL_Delay(100); // Never use this except in the most basic testing.
    // CanSystem_SendRaw("081#1122334455667788");
  }
}
