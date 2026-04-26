#ifndef COPY_RENAME_ME_SYSTEM_H
#define COPY_RENAME_ME_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "stm32l4xx.h"

/*
 * Rename this file pair and rename the function prefixes below.
 *
 * Recommended pattern:
 *   my_feature_system.h
 *   my_feature_system.c
 *
 *   bool my_feature_system_init(void);
 *   void my_feature_system_controller(void);
 */


typedef volatile uint32_t* TIMER_CHANNEL;
typedef	uint16_t GPIO_PIN;

//typedef struct{
//
//
//	TIMER_CHANNEL 	channel;
//	TIM_TypeDef* 	instance;
//	GPIO_TypeDef* 	port;
//	GPIO_PIN 		pin;
//
//	uint32_t 		alternate_func;
//
//} PWM_CHANNEL;

typedef struct{

	GPIO_TypeDef* 	ena_port;
	GPIO_PIN 		ena_pin;

	GPIO_TypeDef* 	enb_port;
	GPIO_PIN 		enb_pin;

	GPIO_TypeDef* 	in1_port;
	GPIO_PIN 		in1_pin;

	GPIO_TypeDef* 	in2_port;
	GPIO_PIN 		in2_pin;

	GPIO_TypeDef* 	in3_port;
	GPIO_PIN 		in3_pin;

	GPIO_TypeDef* 	in4_port;
	GPIO_PIN 		in4_pin;



	uint16_t steps_per_rev;

    uint32_t tick_accumulator;
    uint32_t ticks_per_step;
	uint16_t current_speed;
	uint8_t max_speed;

	uint8_t current_step;
	uint8_t dir;

	uint32_t max_time_ms; //max time
	uint32_t current_time_running_ms;//how long its been running

	uint8_t status;

}l298n_stepper_driver_t;



/*
 *
 *
 * PUT CONFIG HERE
 *
 *
 */

#define NUM_STEPPERS 1 //replace w number of steppers

//EXAMPLE

extern l298n_stepper_driver_t drivers[NUM_STEPPERS]; // needs to be defined in the .c
extern TIM_TypeDef* global_interrupt_clock;
extern uint32_t l298n_stepper_timer_period_us; //assumes clock speed is 8mhz

#define STEPPER_0_VEL_PARAM_NAME "SCIENCE_STEPPER_PCB_C.stepper_velocity_target_0"
#define STEPPER_1_VEL_PARAM_NAME "SCIENCE_STEPPER_PCB_C.stepper_velocity_target_1"


/*
 * Initializes any one-time state for the system.
 *
 * Returns:
 *   true  -> init succeeded
 *   false -> init failed
 *
 * Notes:
 * - Safe to call once at startup.
 * - You can also lazy-init from the controller if you prefer.
 */

/*
 * Main scheduler callback for this system.
 *
 * Add this to main.c with:
 *   RR_AddController(copy_rename_me_system_controller);
 *
 * Notes:
 * - This should be non-blocking.
 * - Avoid HAL_Delay() here.
 * - Use HAL_GetTick() for timed behavior.
 */
void l298n_stepper_system_controller(void);

#ifdef __cplusplus
}
#endif

#endif /* COPY_RENAME_ME_SYSTEM_H */
