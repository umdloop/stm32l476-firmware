#include "can_config.h"
#include "project_config.h"

/*
 * Edit this allowlist to restrict which standard arbitration IDs are decoded
 * into parameters.
 *
 * If PROJECT_CAN_USE_DEFAULT_RX_FILTER == 1, the default list below keeps the
 * build focused on the message families used by the shipped examples and core
 * systems. Set the macro to 0 to accept all standard IDs in hardware.
 */

#if PROJECT_CAN_USE_DEFAULT_RX_FILTER
const uint32_t g_can_rx_id_filter[] =
{
  PROJECT_CAN_ID_POWER_PCB_C,
  PROJECT_CAN_ID_POWER_PCB_R,
  PROJECT_CAN_ID_SCIENCE_DC_MOTOR_C,
  PROJECT_CAN_ID_SCIENCE_DC_MOTOR_R,
  PROJECT_CAN_ID_SCIENCE_SERVO_PCB_C,
  PROJECT_CAN_ID_SCIENCE_SERVO_PCB_R,
  PROJECT_CAN_ID_SCIENCE_STEPPER_PCB_C,
  PROJECT_CAN_ID_SCIENCE_STEPPER_PCB_R,
};
#else
const uint32_t g_can_rx_id_filter[] =
{
  /* empty = accept all */
	0x080,
	0x010,
};
#endif

const size_t g_can_rx_id_filter_count = (sizeof(g_can_rx_id_filter) / sizeof(g_can_rx_id_filter[0]));
