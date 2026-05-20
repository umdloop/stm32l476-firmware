#include "heartbeat_system.h"

#include "can_params.h"
#include "can_system.h"
#include "main.h"

static uint8_t  s_inited = 0U;
static uint32_t s_heartbeat_timer = 0U;

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

static void init_once(void)
{
    s_heartbeat_timer = HAL_GetTick();
}

void heartbeat_system_controller(void)
{
    if (!s_inited)
    {
        s_inited = 1U;
        init_once();
    }

    /*
     * Heartbeat delay comes from:
     *   POWER_PCB_C.pcb_heartbeat_delay   (seconds)
     */
    int32_t hb_s = 0;
    if (!CanParams_GetInt32("POWER_PCB_C.pcb_heartbeat_delay", &hb_s))
        return;

    if (hb_s <= 0)
        return;

    uint32_t hb_ms = (uint32_t)hb_s * 1000U;

    if (delay_elapsed(&s_heartbeat_timer, hb_ms))
    {
        /*
         * Send heartbeat response.
         * Legacy API does: set + schedule TX immediately.
         */
        (void)CanParams_SetBool("POWER_PCB_C.pcb_heartbeat_success", true);
        (void)CanSystem_Send("POWER_PCB_C.pcb_heartbeat_success");
    }
}
