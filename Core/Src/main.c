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


	/* --- NEMA 17 HARD-CODE START (Corrected for 8MHz Clock) --- */

	// 1. ENABLE MOTORS (Logic LOW = ON for NEMA 17 drivers)
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4,  GPIO_PIN_RESET); // enbl_0
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); // enbl_1
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3,  GPIO_PIN_RESET); // enbl_2
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4,  GPIO_PIN_RESET); // enbl_3
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8,  GPIO_PIN_RESET); // enbl_4
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET); // enbl_5

	// 2. SET DIRECTIONS (Logic HIGH)
	HAL_GPIO_WritePin(PD2,  GPIO_PIN_2,  GPIO_PIN_SET);   // dir_0
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6,  GPIO_PIN_SET);   // dir_1
	HAL_GPIO_WritePin(PC14, GPIO_PIN_14, GPIO_PIN_SET);   // dir_2
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2,  GPIO_PIN_SET);   // dir_3
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7,  GPIO_PIN_SET);   // dir_4
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2,  GPIO_PIN_SET);   // dir_5

	// 3. CONFIGURE SPEED
	// With Prescaler=7 and SYSCLK=8MHz, Timer clock is 1MHz.
	// ARR = 2000 gives 500 pulses per second (500Hz).
	uint32_t freq_arr = 2000;
	uint32_t duty_cycle = 1000; // 50% duty

	__HAL_TIM_SET_AUTORELOAD(&htim2, freq_arr);
	__HAL_TIM_SET_AUTORELOAD(&htim3, freq_arr);
	__HAL_TIM_SET_AUTORELOAD(&htim4, freq_arr);

	// 4. START PWM CHANNELS (Based on your .ioc map)
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2); // step_0 (PB3)
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2); // step_1 (PB5)
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3); // step_2 (PB8)
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1); // step_3 (PA5)
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3); // step_4 (PB0)
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3); // step_5 (PB10)

	// 5. APPLY DUTY CYCLE
	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, duty_cycle);
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, duty_cycle);
	__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, duty_cycle);
	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, duty_cycle);
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, duty_cycle);
	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, duty_cycle);

  /* Initialize App Systems */
 // RR_Scheduler_Init();

  /* Register Controllers */

  //RR_AddController(can_system_controller);
  //RR_AddController(pcb_led_system_controller);
  //RR_AddController(heartbeat_system_controller);
  //RR_AddController(stepper_system_controller);


  while (1)
  {
    RR_Scheduler_Tick();
  }
}
