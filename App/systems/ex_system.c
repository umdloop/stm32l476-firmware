#include "ex_system.h"
#include "app_config.h"
#include "can_params.h"
#include "can_system.h"
#include "main.h"

static uint8_t s_inited = 0U;

static void init_once(void)
{
  /* one-time init */
}

void ex_system_controller(void)
{
  if (!s_inited)
  {
    s_inited = 1U;
    init_once();
  }

  /* do small amount of work and return */
}
