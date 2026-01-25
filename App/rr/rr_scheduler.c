#include "rr_scheduler.h"

#include <stddef.h>

#define RR_MAX_CONTROLLERS (16U)

static rr_controller_t s_controllers[RR_MAX_CONTROLLERS];
static size_t s_controller_count = 0;

void RR_Scheduler_Init(void)
{
  for (size_t i = 0; i < RR_MAX_CONTROLLERS; i++)
  {
    s_controllers[i] = NULL;
  }
  s_controller_count = 0;
}

bool RR_AddController(rr_controller_t controller)
{
  if (controller == NULL)
  {
    return false;
  }

  /* Already present? */
  for (size_t i = 0; i < s_controller_count; i++)
  {
    if (s_controllers[i] == controller)
    {
      return true;
    }
  }

  if (s_controller_count >= RR_MAX_CONTROLLERS)
  {
    return false;
  }

  s_controllers[s_controller_count++] = controller;
  return true;
}

bool RR_RemoveController(rr_controller_t controller)
{
  if (controller == NULL)
  {
    return false;
  }

  for (size_t i = 0; i < s_controller_count; i++)
  {
    if (s_controllers[i] == controller)
    {
      /* Shift down */
      for (size_t j = i; j + 1 < s_controller_count; j++)
      {
        s_controllers[j] = s_controllers[j + 1];
      }
      s_controllers[s_controller_count - 1] = NULL;
      s_controller_count--;
      return true;
    }
  }

  return false;
}

void RR_Scheduler_Tick(void)
{
  for (size_t i = 0; i < s_controller_count; i++)
  {
    if (s_controllers[i])
    {
      s_controllers[i]();
    }
  }
}
