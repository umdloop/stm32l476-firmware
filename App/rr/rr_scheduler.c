#include "rr_scheduler.h"

#include <stddef.h>
#include "app_config.h"

#if SYSTEM_BLINK_ENABLED
#include "blink_system.h"
#endif

typedef void (*rr_init_fn_t)(void);
typedef void (*rr_tick_fn_t)(void);

typedef struct
{
  rr_init_fn_t init;
  rr_tick_fn_t tick;
} rr_system_t;

static rr_system_t s_systems[8];
static size_t s_system_count = 0;

static void RR_Register(rr_init_fn_t init_fn, rr_tick_fn_t tick_fn)
{
  if (s_system_count < (sizeof(s_systems) / sizeof(s_systems[0])))
  {
    s_systems[s_system_count].init = init_fn;
    s_systems[s_system_count].tick = tick_fn;
    s_system_count++;
  }
}

void RR_Scheduler_Init(void)
{
  s_system_count = 0;

#if SYSTEM_BLINK_ENABLED
  RR_Register(BlinkSystem_Init, BlinkSystem_Tick);
#endif

  for (size_t i = 0; i < s_system_count; i++)
  {
    if (s_systems[i].init)
    {
      s_systems[i].init();
    }
  }
}

void RR_Scheduler_Tick(void)
{
  for (size_t i = 0; i < s_system_count; i++)
  {
    if (s_systems[i].tick)
    {
      s_systems[i].tick();
    }
  }
}
