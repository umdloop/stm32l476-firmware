#include "pcb_led_system.h"

#include "app_config.h"
#include "can_params.h"
#include "main.h"

static uint8_t s_inited = 0;

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

  /* Only act when signal exists+valid. If not valid yet, keep OFF. */
  if (CanParams_GetBool("STEPPER_COMMAND.Set_LED", &on))
  {
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
  }
  else
  {
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);
  }
}
