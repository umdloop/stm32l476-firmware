#include "ex_system.h"
#include "app_config.h"
#include "can_params.h"
#include "can_system.h"
#include "main.h"

#define MARGIN_OF_ERROR 5
#define MOTOR_POSITION_REQUEST_PARAMETER "STEPPER_PCB_C.position_request"

static uint8_t s_inited = 0U;

static void init_once(void)
{
  /* one-time init */
  // Figure out how to init pins and setup any needed timers, etc. etc.
  // Potential solution to ugly return
  // CanParams_SetBool("STEPPER_PCB_C.shutdown_status", false);
}

void stepper_system_controller(void)
{
  if (!s_inited)
  {
    s_inited = 1U;
    init_once();
  }

  /* do small amount of work and return */
  // This is where we want to actually figure out how to control the stepper motors by handling the CAN commands.

  // STEPPER POS GRABBER
  int32_t actual_Position = 0;
  bool position_Requested = false;

  if (!CanParams_GetBool(MOTOR_POSITION_REQUEST_PARAMTER, &position_Requested)) {
	  // Prolly don't want to return here.
  } else {
	  if(position_Requested) {
		  CanSystem_SetInt32("STEPPER_PCB_R.stepper_position", &position_Requested);
	  }
  }

  // STEPPER SHUTDOWN HANDELER
  bool is_Shutdown = false;


  if (!CanParams_GetBool("STEPPER_PCB_C.shutdown_status", &is_Shutdown)) {
	  // This condition returns the controller (AKA skips it in the RR),
	  // if the CanParams function is false (AKA failed to grab shutdown_status from database)
	  // It can fail if the value was never set in the DB, or for any weird reasons too.

	  // EX: MAYBE? BAD PLACE TO PUT RETURN
	  //return;
  }

  if (is_Shutdown) {
	  // This code here should manipulate pins to somehow shutdown the motor.
  }

  // STEPPER POSITION REQUEST HANDLER
  int32_t desired_Position = 0;

  if (!CanParams_GetInt32("STEPPER_PCB_C.position_request", &desired_Position)) {
	  // This condition returns the controller (AKA skips it in the RR),
	  // if the CanParams function is false (AKA failed to grab shutdown_status from database)
	  // It can fail if the value was never set in the DB, or for any weird reasons too.
	  return;
  }

  // Do something
  if (abs((actual_Position - desired_Position)) > MARGIN_OF_ERROR) {
	  // Do something that moves motor, BUT is NOT BLOCKING
	  // QUICK ACTIVATE
  } else {
	  // QUICK DEACTIVATE
  }

}
