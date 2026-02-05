#include "main.h"

/* Platform */
#include "../../Platform/Inc/system_clock.h"
#include "../../Platform/Inc/gpio.h"
#include "../../Platform/Inc/can.h"
#include "../../Platform/Inc/usart.h"
#include "../../Platform/Inc/tim.h"
/* App */
#include "../../App/Inc/rr_scheduler.h"
#include "../../App/Inc/can_system.h"
#include "../../App/Inc/pcb_led_system.h"
#include "../../App/Inc/heartbeat_system.h"
#include "../../App/Inc/stepper_system.h"

int main(void)
{
  HAL_Init();

  Platform_SystemClock_Config();

  MX_GPIO_Init();
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_SET);
  MX_CAN1_Init();
  MX_UART4_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);

  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);

  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);

  RR_Scheduler_Init();

  RR_AddController(can_system_controller);

  RR_AddController(pcb_led_system_controller);

  RR_AddController(heartbeat_system_controller);

  RR_AddController(stepper_system_controller);
  HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_10);
      HAL_Delay(1000);
      HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_10);
      HAL_Delay(1000);




  while (1)
  {
	    // === ENABLE ALL MOTORS (active-low) ===
	    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);  // Motor 0
	    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); // Motor 1
	    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);  // Motor 2
	    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);  // Motor 3
	    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_RESET);  // Motor 4
	    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET); // Motor 5

	    // === SET DIRECTION (all forward) ===
	    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET); // Motor 0
	    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); // Motor 1
	    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, GPIO_PIN_SET); // Motor 2
	    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET); // Motor 3
	    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET); // Motor 4
	    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET); // Motor 5

	    // === SET STEP DUTY (50%) ===
	    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 500); // Motor 0
	    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 500); // Motor 1
	    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, 500); // Motor 2
	    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 500); // Motor 3
	    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 500); // Motor 4
	    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 500); // Motor 5
    //RR_Scheduler_Tick();
  }
}
