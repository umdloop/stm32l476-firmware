#include "main.h"
#include <stdio.h>

/* Platform */
#include "../../Platform/Inc/system_clock.h"
#include "../../Platform/Inc/gpio.h"
#include "../../Platform/Inc/can.h"
#include "../../Platform/Inc/usart.h"

/* App */
#include "../../App/Inc/rr_scheduler.h"
#include "../../App/Inc/can_system.h"
#include "../../App/Inc/gpio_system.h"
#include "../../App/Inc/heartbeat_system.h"

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
  RR_AddController(can_system_controller); // Needed for CAN communication

  /* Core application systems */
  RR_AddController(gpio_system_controller); // Runs onboard LED and standard GPIO functionality
  RR_AddController(heartbeat_system_controller);

  RR_Scheduler_Tick(); // One tick to setup all the inits.

  // System Specific Assignments
  GpioSystem_DigitalAssign(5, 'C', "POWER_PCB_C.pcb_led_status");


  while (1)
  {
    RR_Scheduler_Tick();
  }
}
