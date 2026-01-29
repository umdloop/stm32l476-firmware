#include "ex_system.h"
#include "app_config.h"
#include "can_params.h"
#include "can_system.h"
#include "main.h"

static uint8_t  s_inited = 0U;
static uint32_t heartbeat_timer = 0U;

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
    /* one-time init */
}

void heartbeat_system_controller(void)
{
    if (!s_inited)
    {
        s_inited = 1U;
        heartbeat_timer = HAL_GetTick();
        init_once();
    }

    if (!CanParams_IsValid("SERVO_PCB_C.status_heartbeat_delay"))
        return;

    int32_t hb_s = 0;
    if (!CanParams_GetInt32("SERVO_PCB_C.status_heartbeat_delay", &hb_s) || hb_s <= 0)
        return;

    uint32_t hb_ms = (uint32_t)hb_s * 1000U;

    if (delay_elapsed(&heartbeat_timer, hb_ms))
    {
        (void)CanSystem_SetInt32("SERVO_PCB_R.status_0", 0);
    }
}

