#include "analog_read_system.h"
#include "gpio_system.h"

#include <stdbool.h>
#include <stdint.h>

/* HAL / platform */
#include "stm32l4xx_hal.h"

/* CAN API */
#include "can_system.h"
#include "can_params.h"

static bool s_initialized = false;
static uint32_t s_last_tick_ms = 0U;

bool analog_read_system_init(void) {

  s_initialized = true;
  s_last_tick_ms = HAL_GetTick();

  return true;
}

bool readDiodeVoltage = false;
int diodeVoltage = 0;

void analog_read_system_controller(void)
{
  if (!s_initialized)
  {
    if (!analog_read_system_init())
    {
      return;
    }
  }
  // CanSystem_SendRaw("000#00");
  if (!CanParams_GetBool("FLUOROMETRY_PCB_C.diode_req_event_0", &readDiodeVoltage)) {
	  CanSystem_SendRaw("001#00");
	  if (readDiodeVoltage) {
		  CanSystem_SendRaw("002#00");
		  diodeVoltage = GpioSystem_AnalogRead(5, 'C', 1024);
		  CanSystem_SendRaw("003#00");
		  CanParams_SetInt32("FLUOROMETRY_PCB_R.diode_value_0", diodeVoltage);
		  CanSystem_SendRaw("004#00");
		  CanSystem_Send("FLUOROMETRY_PCB_R.diode_value_0");
		  CanSystem_SendRaw("005#00");
	  }
  } else {
	  CanSystem_SendRaw("000#01");
  }
  readDiodeVoltage = false;
}
