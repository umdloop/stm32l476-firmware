#include "blink_system.h"

#include "app_config.h"

static uint32_t s_last_toggle_ms = 0;

void BlinkSystem_Init(void)
{
  s_last_toggle_ms = HAL_GetTick();
}

void BlinkSystem_Tick(void)
{
  const uint32_t now = HAL_GetTick();
  if ((now - s_last_toggle_ms) >= BLINK_PERIOD_MS)
  {
    s_last_toggle_ms = now;
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
  }
}
