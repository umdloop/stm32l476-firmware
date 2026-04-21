#include "copy_rename_me_system.h"

#include <stdbool.h>
#include <stdint.h>

/* HAL / platform */
#include "stm32l4xx_hal.h"

/* CAN API */
#include "can_system.h"
#include "can_params.h"

/*
 * ============================================================================
 *  Rename notes
 * ============================================================================
 *
 * Rename:
 *   copy_rename_me_system_init()
 *   copy_rename_me_system_controller()
 *
 * and the filenames:
 *   copy&renameME_system.h
 *   copy&renameME_system.c
 *
 * Suggested example:
 *   dc_motor_test_system.h
 *   dc_motor_test_system.c
 *   dc_motor_test_system_init()
 *   dc_motor_test_system_controller()
 */

/*
 * ============================================================================
 *  Private state
 * ============================================================================
 */

static bool s_initialized = false;
static uint32_t s_last_tick_ms = 0U;

/*
 * ============================================================================
 *  Init
 * ============================================================================
 */

bool copy_rename_me_system_init(void)
{
  /*
   * Put one-time setup here.
   *
   * Examples:
   * - initialize local state
   * - initialize platform peripherals used only by this system
   * - write default CAN parameter values
   */

  s_initialized = true;
  s_last_tick_ms = HAL_GetTick();

  return true;
}

/*
 * ============================================================================
 *  Controller
 * ============================================================================
 */

void copy_rename_me_system_controller(void)
{
  /*
   * Lazy init pattern:
   * If you want the system to self-start when first scheduled, keep this.
   * Otherwise you can call init from main.c and remove this block.
   */
  if (!s_initialized)
  {
    if (!copy_rename_me_system_init())
    {
      return;
    }
  }

  /*
   * Example non-blocking periodic logic.
   * Runs every 100 ms.
   */
  uint32_t now = HAL_GetTick();
  if ((now - s_last_tick_ms) < 100U)
  {
    return;
  }
  s_last_tick_ms = now;

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
}

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
