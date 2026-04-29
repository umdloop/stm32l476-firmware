//#include "copy_rename_me_system.h"

#include <stdbool.h>
#include <stdint.h>

/* HAL / platform */
#include "stm32l4xx_hal.h"

/* CAN API */
#include "can_system.h"
#include "can_params.h"




/*
 * =======================================================================
 * ASSUMES THE FOLLOWING
 * 		MAX OF 16 STEPPERS
 * 		ASSUMES USER DID NOT REPEAT DEFINITIONS FOR PINOUT
 * 		OTHER ASSUMPTIONS CAN BE DERIVED FROM STRUCTS in .h
 *========================================================================
 */

/*
 * ============================================================================
 *  Private state
 * ============================================================================
 */

#include "l298n_stepper_system.h"

#define L298N_STEPPER_STATUS_UNDEFINED 		(uint8_t)0
#define L298N_STEPPER_STATUS_IDLE 			(uint8_t)1
#define L298N_STEPPER_STATUS_STARTUP		(uint8_t)2
#define L298N_STEPPER_STATUS_ERR_INVREQ		(uint8_t)3
#define L298N_STEPPER_STATUS_ERR_DISARM 	(uint8_t)4
#define L298N_STEPPER_STATUS_ERR_FAIL 		(uint8_t)5
#define L298N_STEPPER_STATUS_ERR_CONTRFAIL	(uint8_t)6 	//probably unused
#define L298N_STEPPER_STATUS_ERR_ESTOP		(uint8_t)7	//probably unused
#define L298N_STEPPER_STATUS_ERR_POS_UNKOWN (uint8_t)8
#define L298N_STEPPER_STATUS_POS_CONTR		(uint8_t)9
#define L298N_STEPPER_STATUS_VEL_CONTR		(uint8_t)10
#define L298N_STEPPER_STATUS_STOPPED		(uint8_t)11
/*
 * ===============================================================
 * CONFIG
 * ===============================================================
 */

char* vel_param_names[NUM_STEPPERS] = {};
char* status_param_names_R[NUM_STEPPERS] = {};
char* status_param_names_C[NUM_STEPPERS] = {};


l298n_stepper_driver_t drivers[NUM_STEPPERS] = {


//		{GPIOA, GPIO_PIN_6,
//		GPIOA, GPIO_PIN_5,
//		GPIOC, GPIO_PIN_4,
//		GPIOA, GPIO_PIN_4,
//		GPIOC, GPIO_PIN_5,
//		GPIOA, GPIO_PIN_7,
//
//		400,
//		0, (1<<31),0, 912,
//		0,0,
//		0,0, L298N_STEPPER_STATUS_UNDEFINED
//		}
//
//
//
//
//
//
//






}; // needs to be defined in the .c
TIM_TypeDef* global_interrupt_clock = TIM2;
uint32_t l298n_stepper_timer_period_us = 100;








void step_0(l298n_stepper_driver_t* l298n){



	HAL_GPIO_WritePin(l298n->in1_port,l298n->in1_pin, SET);
	HAL_GPIO_WritePin(l298n->in2_port,l298n->in2_pin,  RESET);

	HAL_GPIO_WritePin(l298n->in3_port,l298n->in3_pin, SET);
	HAL_GPIO_WritePin(l298n->in4_port,l298n->in4_pin,  RESET);
}

void step_1(l298n_stepper_driver_t* l298n){



	HAL_GPIO_WritePin(l298n->in1_port,l298n->in1_pin, RESET);
	HAL_GPIO_WritePin(l298n->in2_port,l298n->in2_pin,  SET);

	HAL_GPIO_WritePin(l298n->in3_port,l298n->in3_pin, SET);
	HAL_GPIO_WritePin(l298n->in4_port,l298n->in4_pin,  RESET);
}

void step_2(l298n_stepper_driver_t* l298n){

	HAL_GPIO_WritePin(l298n->in1_port,l298n->in1_pin, RESET);
	HAL_GPIO_WritePin(l298n->in2_port,l298n->in2_pin,  SET);

	HAL_GPIO_WritePin(l298n->in3_port,l298n->in3_pin, RESET);
	HAL_GPIO_WritePin(l298n->in4_port,l298n->in4_pin,  SET);

}

void step_3(l298n_stepper_driver_t* l298n){

	HAL_GPIO_WritePin(l298n->in1_port,l298n->in1_pin, SET);
	HAL_GPIO_WritePin(l298n->in2_port,l298n->in2_pin,  RESET);

	HAL_GPIO_WritePin(l298n->in3_port,l298n->in3_pin, RESET);
	HAL_GPIO_WritePin(l298n->in4_port,l298n->in4_pin,  SET);


}

void (*steps[4])(l298n_stepper_driver_t* l298n) = {step_0, step_1, step_2, step_3};





static void enable_timer_nvic(TIM_TypeDef* tim) {
    IRQn_Type irqn;

    if      (tim == TIM1)  irqn = TIM1_UP_TIM16_IRQn;
    else if (tim == TIM2)  irqn = TIM2_IRQn;
    else if (tim == TIM3)  irqn = TIM3_IRQn;
    else if (tim == TIM4)  irqn = TIM4_IRQn;
    else if (tim == TIM5)  irqn = TIM5_IRQn;
    else if (tim == TIM6)  irqn = TIM6_DAC_IRQn;
    else if (tim == TIM7)  irqn = TIM7_IRQn;
    else return; // unsupported

    NVIC_SetPriority(irqn, 1);
    NVIC_EnableIRQ(irqn);
}
static void enable_timer_clock(TIM_TypeDef* tim) {
    if      (tim == TIM1)  __HAL_RCC_TIM1_CLK_ENABLE();
    else if (tim == TIM2)  __HAL_RCC_TIM2_CLK_ENABLE();
    else if (tim == TIM3)  __HAL_RCC_TIM3_CLK_ENABLE();
    else if (tim == TIM4)  __HAL_RCC_TIM4_CLK_ENABLE();
    else if (tim == TIM5)  __HAL_RCC_TIM5_CLK_ENABLE();
    else if (tim == TIM6)  __HAL_RCC_TIM6_CLK_ENABLE();
    else if (tim == TIM7)  __HAL_RCC_TIM7_CLK_ENABLE();
    else return;
    // etc...
}
static void init_global_clk(void){

	enable_timer_clock(global_interrupt_clock);
	global_interrupt_clock->PSC = 7;
	global_interrupt_clock->ARR = l298n_stepper_timer_period_us -1;
    global_interrupt_clock->CR1 |= TIM_CR1_ARPE;
    global_interrupt_clock->DIER |= TIM_DIER_UIE;
    global_interrupt_clock->SR &= ~TIM_SR_UIF;
    global_interrupt_clock->CR1 |= TIM_CR1_CEN;


    enable_timer_nvic(global_interrupt_clock);

}
static bool l298n_stepper_system_init(void);

static bool init_l298n(l298n_stepper_driver_t*);
static bool init_gpio(l298n_stepper_driver_t*);






void turn_off_motors(l298n_stepper_driver_t* l298n){

	l298n->status = L298N_STEPPER_STATUS_IDLE;

	HAL_GPIO_WritePin(l298n->ena_port, l298n->ena_pin, RESET);
	HAL_GPIO_WritePin(l298n->enb_port, l298n->enb_pin, RESET);
}
void turn_on_motors(l298n_stepper_driver_t* l298n){

	l298n->status = L298N_STEPPER_STATUS_VEL_CONTR;

	HAL_GPIO_WritePin(l298n->ena_port, l298n->ena_pin, SET);
	HAL_GPIO_WritePin(l298n->enb_port, l298n->enb_pin, SET);



}


void do_step(l298n_stepper_driver_t* l298n){


	l298n->current_step += l298n->dir;
	l298n->current_step &= 0b011;
	steps[l298n->current_step](l298n);



}

void stepper_tick(l298n_stepper_driver_t* stepper) {
    stepper->tick_accumulator++;
    if (stepper->tick_accumulator >= stepper->ticks_per_step) {
        stepper->tick_accumulator = 0;
        do_step(stepper);
    }
}

static void stepper_irq_handler(void) {
    if (global_interrupt_clock->SR & TIM_SR_UIF) {
        global_interrupt_clock->SR &= ~TIM_SR_UIF;
        for (int i = 0; i < NUM_STEPPERS; i++) {
            stepper_tick(&drivers[i]);
        }
    }
}

__weak void TIM1_UP_TIM16_IRQHandler(void) { stepper_irq_handler(); }
__weak void TIM2_IRQHandler(void)          { stepper_irq_handler(); }
__weak void TIM3_IRQHandler(void)          { stepper_irq_handler(); }
__weak void TIM4_IRQHandler(void)          { stepper_irq_handler(); }
__weak void TIM5_IRQHandler(void)          { stepper_irq_handler(); }
__weak void TIM6_DAC_IRQHandler(void)      { stepper_irq_handler(); }
__weak void TIM7_IRQHandler(void)          { stepper_irq_handler(); }


/*
 * ============================================================================
 *  Init
 * ============================================================================
 */
static bool l298n_stepper_system_init(void){


	init_global_clk();


	for(int i = 0; i < NUM_STEPPERS; i++){

		if(!init_l298n((l298n_stepper_driver_t*)&drivers[i])) return false;
	}



	return true;

}

static bool init_l298n(l298n_stepper_driver_t* l298n){


	l298n->status = L298N_STEPPER_STATUS_STARTUP;


	if (!init_gpio(l298n)) {

		l298n->status = L298N_STEPPER_STATUS_ERR_FAIL;

		return false;

	}

	l298n->status = L298N_STEPPER_STATUS_IDLE;

	return true;

}

static bool init_gpio(l298n_stepper_driver_t*l298n){

	//constants used, help with iteration
	const int num_ports = 8;
	const uint32_t port_dist = ((uint32_t)GPIOB-(uint32_t)GPIOA);


	//put all the GPIOs that will be used in an array for iteration
	GPIO_TypeDef* ports[6] = {l298n->ena_port,l298n->enb_port,l298n->in1_port,l298n->in2_port,l298n->in3_port,l298n->in4_port};
	GPIO_PIN pins[6] = {l298n->ena_pin,l298n->enb_pin,l298n->in1_pin,l298n->in2_pin,l298n->in3_pin,l298n->in4_pin};


	//tracks ports we've seen, and the pins needed for each port
	bool ports_seen[num_ports] = {};
	GPIO_PIN pins_for_port[num_ports] = {};

	//iterate through all the ports we will use, set seen flag and |= the pins
	uint8_t index = 0;
	for (int i = 0; i < 6; i++){

		//index offset, verified constant address difference between the port base addr (port_dist)
		index = ((uint32_t)ports[i] - (uint32_t)GPIOA)/port_dist;

		if (index > num_ports-1) return false;

		//set flag
		ports_seen[index] = true;
		pins_for_port[index] |= pins[i];

	}



	//start initialize
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	//iterate through all ports
	for (int i = 0; i< num_ports; i++){

		//if we need to initialize the port
		if(ports_seen[i]){

			//case statement unavoidable - the RCC enables are macro'd, so can't do function pointer array :(
			switch(i){
			 	 case 0: __HAL_RCC_GPIOA_CLK_ENABLE(); break;
			 	 case 1: __HAL_RCC_GPIOB_CLK_ENABLE(); break;
			 	 case 2: __HAL_RCC_GPIOC_CLK_ENABLE(); break;
			 	 case 3: __HAL_RCC_GPIOD_CLK_ENABLE(); break;
			 	 case 4: __HAL_RCC_GPIOE_CLK_ENABLE(); break;
			 	 case 5: __HAL_RCC_GPIOF_CLK_ENABLE(); break;
			 	 case 6: __HAL_RCC_GPIOG_CLK_ENABLE(); break;
			 	 case 7: __HAL_RCC_GPIOH_CLK_ENABLE(); break;

			}

		//initialize the port and pin
		/*
		 * OFFSET MATH: convert GPIO (index 0) to uint32_t to avoid pointer arithmetic
		 * 				add index*offset
		 * 				done
		 *
		 * 				Pointer arithmetic needs to be avoided because the GPIO_TypeDef is
		 * 				much smaller than the actual memory dedicated for each GPIO port (1KB = 0x0400)
		 */

		HAL_GPIO_WritePin((GPIO_TypeDef*)((uint32_t)GPIOA + i*port_dist), pins_for_port[i], GPIO_PIN_RESET);
		GPIO_InitStruct.Pin = pins_for_port[i];
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_PULLUP;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init((GPIO_TypeDef*)((uint32_t)GPIOA + i*port_dist), &GPIO_InitStruct);


		 }

	 }

	 return true;


}


static bool update_speed(l298n_stepper_driver_t* l298n, int16_t speed){

	if (speed < 0){
		if (-1*speed > l298n->max_speed) return false;
		l298n->dir = -1;
		speed = -1*speed;
	}
	else if(speed > 0){
		if (speed > l298n->max_speed) return false;
		l298n->dir = 1;
	}
	else{
		l298n->dir = 0; l298n->ticks_per_step = (1<<31);
		turn_off_motors(l298n); return true;
	}

	l298n->ticks_per_step = (uint32_t)(
	    (1000000.0f / l298n_stepper_timer_period_us) /
	    ((float)speed / 360.0f * l298n->steps_per_rev));
		turn_on_motors(l298n);
	return true;
}




bool is_init = false;
void l298n_stepper_system_controller(void){
	if (is_init == false){l298n_stepper_system_init(); is_init = true;}

	bool request_status = false;
	bool flag = false;



	//update speeds of motors, only if they were sucessfully initialized
	for (int i = 0; i < NUM_STEPPERS; i++){

		int16_t speed = 0;

		bool valid  = CanParams_GetInt32(vel_param_names[i], (int32_t*)&speed);

		if(valid && (speed == 900 || speed == -900 || speed == 0)){

			if (drivers[i].status != L298N_STEPPER_STATUS_ERR_FAIL)
				update_speed((l298n_stepper_driver_t*)&drivers[i], speed);


		}


		if(CanParams_ProcEvent(status_param_names_C[i], &flag) && flag){

			CanParams_GetBool(status_param_names_C[i], (int32_t*)&request_status);

			if (request_status){

				(void)CanParams_SetInt32(status_param_names_R[i], (int32_t)(drivers[i].status));
				(void)CanSystem_Send(status_param_names_R[i]);

			}


		}



	}


}

