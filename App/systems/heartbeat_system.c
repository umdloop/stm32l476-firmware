#include "heartbeat_system.h" // Changed from ex_system.h to match your probable header
#include "app_config.h"
#include "can_params.h"
#include "can_system.h"
#include "main.h"
#include "../../Platform/Inc/gpio.h"
#include "stm32l4xx_hal.h"

static uint8_t  s_inited = 0U;
static uint32_t heartbeat_timer = 0U;
static uint8_t  s_hb_delay_sec = 1U; // Default to 1s as per protocol range

static bool delay_elapsed(uint32_t *timestamp, uint32_t delay_ms)
{
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - *timestamp) >= delay_ms)
    {
        *timestamp = now;
        return true;
    }
    return false;
}

void heartbeat_system_controller(void)
{
    if (!s_inited)
    {
        s_inited = 1U;
        heartbeat_timer = HAL_GetTick();
    }

    // 1. Check for rate updates from the CAN bus (Command 0x10)
    // Note: In your architecture, the CAN system likely updates these params automatically
    int32_t requested_delay = 0;
    if (CanParams_GetInt32("STEPPER_PCB_C.pcb_heartbeat_delay", &requested_delay))
    {
        if (requested_delay >= 0 && requested_delay <= 255)
        {
            s_hb_delay_sec = (uint8_t)requested_delay;
        }
    }

    // 2. Handle the periodic heartbeat transmission
    if (s_hb_delay_sec > 0)
    {
        uint32_t hb_ms = (uint32_t)s_hb_delay_sec * 1000U;

        if (delay_elapsed(&heartbeat_timer, hb_ms))
        {
            // Send heartbeat command to stepper PCB on 0x90
            CanSystem_SetInt32("STEPPER_PCB_C.cmd", 16);  // Heartbeat command
            CanSystem_SetInt32("STEPPER_PCB_C.pcb_heartbeat_delay", s_hb_delay_sec);

            // Toggle LED (use PC5 as a known onboard LED pin)
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_5);
        }
    }
}
