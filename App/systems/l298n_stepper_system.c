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


/*
 * ===============================================================
 * CONFIG
 * ===============================================================
 */


l298n_stepper_driver_t drivers[NUM_STEPPERS] = {


		{GPIOA, GPIO_PIN_6,
		GPIOA, GPIO_PIN_5,
		GPIOC, GPIO_PIN_4,
		GPIOA, GPIO_PIN_4,
		GPIOC, GPIO_PIN_5,
		GPIOA, GPIO_PIN_7,

		400,
		0, (1<<31),0, 912,
		0,0,
		0,0,0
		}













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



void turn_off_motors(l298n_stepper_driver_t* l298n){


	HAL_GPIO_WritePin(l298n->ena_port, l298n->ena_pin, RESET);
	HAL_GPIO_WritePin(l298n->enb_port, l298n->enb_pin, RESET);
}
void turn_on_motors(l298n_stepper_driver_t* l298n){
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


	for(int i = 0; i < NUM_STEPPERS; i++){

		if(!init_l298n(&drivers[i])) return false;
	}

	init_global_clk();
	return true;

}

static bool init_l298n(l298n_stepper_driver_t* l298n){



	if (!init_gpio(l298n)) return false;
	//if (!init_pwm(l298n)) return false;

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

/*
 * We get speed in degrees/sec
 * we have X steps/rev
 * We have sec/tick
 * we need ticks/step
 *
 * (d/s) *1/360(rev/d)*(steps/rev)*(us/tick)*1/1000000(s/us) ^-1
 *1/(us/tick)*1000000(us/s)*
 *
 *
 */

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


	update_speed((l298n_stepper_driver_t*)&drivers, 900);



//	CanPa



















}







/*
 * ============================================================================
 *  Controller
 * ============================================================================
 */

//void copy_rename_me_system_controller(void)
//{
//  /*
//   * Lazy init pattern:
//   * If you want the system to self-start when first scheduled, keep this.
//   * Otherwise you can call init from main.c and remove this block.
//   */
//  if (!s_initialized)
//  {
//    if (!copy_rename_me_system_init())
//    {
//      return;
//    }
//  }

  /*
   * Example non-blocking periodic logic.
   * Runs every 100 ms.
   */
//  uint32_t now = HAL_GetTick();
//  if ((now - s_last_tick_ms) < 100U)
//  {
//    return;
//  }
//  s_last_tick_ms = now;

  /*
   * Put your real recurring logic here.
   *
   * Typical patterns:
   * - read a CAN parameter
   * - update hardware
   * - publish a CAN response
   * - process an event flag
   */

  /* Example placeholder:
   *
   * int32_t value = 0;
   * if (CanParams_GetInt32("SCIENCE_DC_MOTOR_PCB_C.dc_motor_velocity_target_0", &value))
   * {
   *   // do something with value
   * }
   */
//}

/*
 * ============================================================================
 *  CAN API QUICK REFERENCE
 * ============================================================================
 *
 * These are examples only.
 * Uncomment / adapt what you need.
 *
 * General rule in this repo:
 *   Preferred modern TX pattern:
 *     1) Set/update parameter
 *     2) Schedule send with CanSystem_Send()
 *
 *   Legacy combined helpers also exist:
 *     CanSystem_SetBool / SetInt32 / SetFloat
 *
 * ============================================================================
 */

/*
--------------------------------------------------------------------------------
1) READ A BOOL PARAMETER
--------------------------------------------------------------------------------

bool led_on = false;
if (CanParams_GetBool("POWER_PCB_C.pcb_led_status", &led_on))
{
  // led_on now contains the current stored value
}

Notes:
- Use this when a DBC signal is 1 bit / boolean.
- Returns false if the parameter name is wrong or not found.
*/

/*
--------------------------------------------------------------------------------
2) READ AN INT32 PARAMETER
--------------------------------------------------------------------------------

int32_t motor_cmd = 0;
if (CanParams_GetInt32("SCIENCE_DC_MOTOR_PCB_C.dc_motor_velocity_target_0", &motor_cmd))
{
  // motor_cmd now has the latest decoded value
}

Notes:
- Most integer signals in your repo will be read this way.
- Signedness/scaling are handled by the CAN decode layer before storage.
*/

/*
--------------------------------------------------------------------------------
3) READ A FLOAT PARAMETER
--------------------------------------------------------------------------------

float value = 0.0f;
if (CanParams_GetFloat("SOME_MESSAGE.some_scaled_signal", &value))
{
  // value now has the physical/scaled value
}

Notes:
- Only use this if the DBC signal is stored as float in the param DB.
- Typically signals with non-1 factor or non-0 offset become float params.
*/

/*
--------------------------------------------------------------------------------
4) WRITE / SET A BOOL PARAMETER
--------------------------------------------------------------------------------

(void)CanParams_SetBool("POWER_PCB_R.pcb_led_success", true);

Notes:
- This only updates the stored parameter.
- It does NOT transmit by itself.
- To actually send the containing CAN message, also call CanSystem_Send().
*/

/*
--------------------------------------------------------------------------------
5) WRITE / SET AN INT32 PARAMETER
--------------------------------------------------------------------------------

(void)CanParams_SetInt32("SCIENCE_DC_MOTOR_PCB_R.dc_motor_status_0", 1);

Notes:
- Preferred modern workflow is:
- set param first
- then schedule TX
*/

/*
--------------------------------------------------------------------------------
6) WRITE / SET A FLOAT PARAMETER
--------------------------------------------------------------------------------

(void)CanParams_SetFloat("SOME_MESSAGE.some_scaled_signal", 12.5f);

Notes:
- Same idea as int/bool: this updates stored state only.
*/

/*
--------------------------------------------------------------------------------
7) SEND A MESSAGE / PAGE USING MODERN API
--------------------------------------------------------------------------------

(void)CanParams_SetBool("POWER_PCB_R.pcb_led_success", true);
(void)CanSystem_Send("POWER_PCB_R.pcb_led_success");

Notes:
- For non-mux messages, sending any signal in that message schedules the full
  message to be transmitted with all current stored signal values.
- For muxed messages, sending a signal schedules the mux page that signal
  belongs to.
- If other fields in that message were never set, they usually transmit as
  their stored defaults (commonly zero).
*/

/*
--------------------------------------------------------------------------------
8) SEND A NON-MUX MESSAGE BY MESSAGE NAME
--------------------------------------------------------------------------------

(void)CanSystem_Send("POWER_PCB_R");

Notes:
- This only works for non-multiplexed messages.
- For muxed messages, use a signal name instead.
*/

/*
--------------------------------------------------------------------------------
9) LEGACY COMBINED SET + SEND HELPERS
--------------------------------------------------------------------------------

(void)CanSystem_SetBool("POWER_PCB_R.pcb_led_success", true);
(void)CanSystem_SetInt32("SCIENCE_DC_MOTOR_PCB_R.dc_motor_status_0", 1);
(void)CanSystem_SetFloat("SOME_MESSAGE.some_scaled_signal", 12.5f);

Notes:
- These are convenient but considered legacy in this repo.
- Preferred style is:
    CanParams_SetX(...)
    CanSystem_Send(...)
- Still useful for quick bring-up/tests.
*/

/*
--------------------------------------------------------------------------------
10) RAW MANUAL CAN SEND
--------------------------------------------------------------------------------

(void)CanSystem_SendRaw("70#30FF7F");

Notes:
- Sends a raw CAN frame directly.
- Format:
    XXX#XXXXXXXXXXXXXXXX
- Examples:
    "70#300000"                // short payload
    "70#30FF7F"                // 3-byte payload
    "123#1122334455667788"     // 8-byte payload
- This bypasses the DBC and parameter DB.
- Great for bring-up and debugging.
- Usually standard 11-bit IDs only unless your implementation was extended.
*/

/*
--------------------------------------------------------------------------------
11) EVENT READ
--------------------------------------------------------------------------------

bool fired = false;
if (CanParams_GetEvent("SCIENCE_SERVO_PCB_C", &fired))
{
  if (fired)
  {
    // event currently set
  }
}

Notes:
- Events are useful for "message received" style behavior.
- In your repo, receiving a message/page can flip an associated event.
*/

/*
--------------------------------------------------------------------------------
12) EVENT PROCESS / CLEAR
--------------------------------------------------------------------------------

if (CanParams_ProcEvent("SCIENCE_SERVO_PCB_C"))
{
  // event was set, and was consumed/cleared
}

Notes:
- Good for one-shot handling:
    if event happened -> act once -> clear
*/

/*
--------------------------------------------------------------------------------
13) EVENT SET
--------------------------------------------------------------------------------

(void)CanParams_SetEvent("MY_MESSAGE", true);

Notes:
- Use only if your code intentionally manipulates events directly.
- Most event behavior in this repo comes from CAN RX or internal linkage.
*/

/*
--------------------------------------------------------------------------------
14) DEBUG: LAST RX TICK
--------------------------------------------------------------------------------

uint32_t tick = 0;
if (CanSystem_DebugGetLastRxTick("SCIENCE_DC_MOTOR_PCB_C.dc_motor_velocity_target_0", &tick))
{
  // tick is HAL_GetTick() timestamp of last RX for that message/page
}

Notes:
- Helpful for timeout detection or "is comms alive?" logic.
*/

/*
--------------------------------------------------------------------------------
15) DEBUG: LAST TX TICK
--------------------------------------------------------------------------------

uint32_t tick = 0;
if (CanSystem_DebugGetLastTxTick("SCIENCE_DC_MOTOR_PCB_R.dc_motor_status_0", &tick))
{
  // tick is HAL_GetTick() timestamp of last TX for that message/page
}

Notes:
- Useful for transmit confirmation timing/debugging.
*/

/*
--------------------------------------------------------------------------------
16) DEBUG: CHECK IF A STANDARD ID IS ALLOWED BY FILTER POLICY
--------------------------------------------------------------------------------

bool allowed = CanSystem_DebugIsStdIdAllowed(0x70);

Notes:
- This checks the repo's allowlist logic.
- Useful when debugging why a frame is ignored.
- Hardware filter and software allowlist are related but not identical in how
  overflow cases are handled.
*/

/*
--------------------------------------------------------------------------------
17) SIMPLE PERIODIC TRANSMIT PATTERN
--------------------------------------------------------------------------------

static uint32_t last_tx = 0;
uint32_t now = HAL_GetTick();

if ((now - last_tx) >= 1000U)
{
  last_tx = now;

  (void)CanParams_SetBool("POWER_PCB_R.pcb_led_success", true);
  (void)CanSystem_Send("POWER_PCB_R.pcb_led_success");
}

Notes:
- Preferred over HAL_Delay().
- Keeps the scheduler responsive.
*/

/*
--------------------------------------------------------------------------------
18) SIMPLE RX-DRIVEN HARDWARE CONTROL PATTERN
--------------------------------------------------------------------------------

int32_t duty_cmd = 0;
if (CanParams_GetInt32("SCIENCE_DC_MOTOR_PCB_C.dc_motor_velocity_target_0", &duty_cmd))
{
  // map duty_cmd into PWM / GPIO / analog output logic here
}

Notes:
- This is the most common "controller" pattern in this repo:
    read param -> drive hardware
*/

/*
--------------------------------------------------------------------------------
19) AVOID THIS INSIDE CONTROLLERS
--------------------------------------------------------------------------------

HAL_Delay(100);

Notes:
- Blocking delays stall the whole round-robin scheduler.
- That pauses CAN RX/TX and all other systems.
- Use HAL_GetTick()-based timing instead.
*/

/*
--------------------------------------------------------------------------------
20) HOW TO ENABLE THIS SYSTEM
--------------------------------------------------------------------------------

In main.c:

  RR_AddController(copy_rename_me_system_controller);

Typical order:
  RR_Scheduler_Init();
  RR_AddController(can_system_controller);
  RR_AddController(copy_rename_me_system_controller);

Notes:
- Keep can_system_controller registered if your system depends on CAN params.
*/
