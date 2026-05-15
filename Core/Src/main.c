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
#include "../../App/Inc/rs485_system.h"
#include "../../App/Inc/servo_system.h"
#include "../../App/Inc/dc_motor_system.h"
#include "../../App/Inc/l298n_stepper_system.h"
#include "../../App/Inc/test_pwm_system.h"
#include "../../App/Inc/copy_rename_me_system.h"

// Basic can testing (can raw)
#include "can_system.h"

int main(void)
{
  HAL_Init();
  Platform_SystemClock_Config();

  MX_GPIO_Init();
  MX_CAN1_Init();
  MX_USART1_UART_Init();
  MX_UART4_Init();

  rs485_transport_config_t rs485_transport = {
    .huart = &huart1,
    .de_gpio_port = GPIOA,
    .de_gpio_pin = GPIO_PIN_8,
    .use_manual_direction = true,
    .uart_timeout_ms = PROJECT_RS485_UART_TIMEOUT_MS
  };
  (void)rs485_system_configure_transport(&rs485_transport);

  RR_Scheduler_Init();
  RR_AddController(can_system_controller); // Needed for CAN communication

  /* Core application systems */
  /* Core application systems */
  RR_AddController(gpio_system_controller); // Runs onboard LED and standard GPIO functionality
  // RR_AddController(heartbeat_system_controller);
  RR_AddController(rs485_system_controller);
  // RR_AddController(servo_system_controller);
  // RR_AddController(dc_motor_system_controller);
  // RR_AddController(l298n_stepper_system_controller);
  // RR_AddController(test_pwm_system_controller);
  // RR_AddController(copy_rename_me_system_controller);


  RR_Scheduler_Tick(); // One tick to setup all the inits.

  // System Specific Assignments
  GpioSystem_DigitalAssign(5, 'C', "POWER_PCB_C.pcb_led_status");

  while (1)
  {
    RR_Scheduler_Tick();
  }
}
