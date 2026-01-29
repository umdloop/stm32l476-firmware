#include "pcb_led_system.h"

#include "app_config.h"
#include "can_params.h"
#include "can_system.h"
#include "main.h"

static uint8_t s_inited = 0U;

static void init_once(void)
{
  /* Default LED OFF until parameter becomes valid */
  HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);
}

void pcb_led_system_controller(void)
{
  if (!s_inited)
  {
    s_inited = 1U;
    init_once();
  }

  bool on = false;
  bool valid = CanParams_GetBool("SERVO_PCB_C.led_status", &on);

  /* Drive the physical LED */
  if (valid)
  {
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
  }
  else
  {
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);
  }

  /*
   * TEST TX:
   * Mirror the incoming LED command onto an outgoing parameter.
   * - If incoming signal is valid: send the same value (on/off)
   * - If invalid: force false
   * Only send when the outgoing desired value changes.
   */
  bool desired_out = (valid && on) ? true : false;

  static uint8_t s_has_sent = 0U;
  static bool s_last_desired = false;

  if (!s_has_sent || (desired_out != s_last_desired))
  {
    s_last_desired = desired_out;
    s_has_sent = 1U;
  }
}
