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
// #include "../../App/Inc/ex_system.h"
#include "../../App/Inc/heartbeat_system.h"

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

  // RR_AddController(ex_system_controller);
  RR_AddController(heartbeat_system_controller);

  while (1)
  {
    RR_Scheduler_Tick();
  }
}
