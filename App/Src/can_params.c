#include "can_params.h"

#include <string.h>
#include <stddef.h>

/* =========================
 *  Internal parameter store
 * ========================= */

typedef struct
{
  char name[64];          /* "MESSAGE.SIGNAL" */
  canp_type_t type;
  volatile uint8_t valid;

  union
  {
    volatile uint8_t b;
    volatile int32_t i32;
    volatile float f;
  } v;
} canp_param_t;

#define CANP_MAX_PARAMS (256U)

static canp_param_t s_params[CANP_MAX_PARAMS];
static size_t s_param_count = 0;

/* Called by CAN system after DBC parse */
void CanParams__Reset(void)
{
  for (size_t i = 0; i < CANP_MAX_PARAMS; i++)
  {
    s_params[i].name[0] = '\0';
    s_params[i].type = CANP_TYPE_INT32;
    s_params[i].valid = 0;
    s_params[i].v.i32 = 0;
  }
  s_param_count = 0;
}

/* Called by CAN system to create parameter entries */
bool CanParams__Create(const char* full_name, canp_type_t type)
{
  if (full_name == NULL || full_name[0] == '\0')
  {
    return false;
  }

  /* Already exists? */
  for (size_t i = 0; i < s_param_count; i++)
  {
    if (strncmp(s_params[i].name, full_name, sizeof(s_params[i].name)) == 0)
    {
      /* If created twice, keep the first type and don't fail */
      return true;
    }
  }

  if (s_param_count >= CANP_MAX_PARAMS)
  {
    return false;
  }

  canp_param_t* p = &s_params[s_param_count++];
  (void)memset(p, 0, sizeof(*p));

  (void)strncpy(p->name, full_name, sizeof(p->name) - 1U);
  p->name[sizeof(p->name) - 1U] = '\0';
  p->type = type;
  p->valid = 0;

  return true;
}

static const canp_param_t* find_param(const char* full_name)
{
  if (full_name == NULL)
  {
    return NULL;
  }

  for (size_t i = 0; i < s_param_count; i++)
  {
    if (strncmp(s_params[i].name, full_name, sizeof(s_params[i].name)) == 0)
    {
      return &s_params[i];
    }
  }
  return NULL;
}

static canp_param_t* find_param_mut(const char* full_name)
{
  return (canp_param_t*)find_param(full_name);
}

bool CanParams_IsValid(const char* full_name)
{
  const canp_param_t* p = find_param(full_name);
  return (p && p->valid) ? true : false;
}

bool CanParams_GetBool(const char* full_name, bool* out_value)
{
  if (out_value == NULL)
  {
    return false;
  }

  const canp_param_t* p = find_param(full_name);
  if (p == NULL || p->type != CANP_TYPE_BOOL || p->valid == 0)
  {
    return false;
  }

  *out_value = (p->v.b != 0);
  return true;
}

bool CanParams_GetInt32(const char* full_name, int32_t* out_value)
{
  if (out_value == NULL)
  {
    return false;
  }

  const canp_param_t* p = find_param(full_name);
  if (p == NULL || p->type != CANP_TYPE_INT32 || p->valid == 0)
  {
    return false;
  }

  *out_value = p->v.i32;
  return true;
}

bool CanParams_GetFloat(const char* full_name, float* out_value)
{
  if (out_value == NULL)
  {
    return false;
  }

  const canp_param_t* p = find_param(full_name);
  if (p == NULL || p->type != CANP_TYPE_FLOAT || p->valid == 0)
  {
    return false;
  }

  *out_value = p->v.f;
  return true;
}

bool CanParams_SetBool(const char* full_name, bool value)
{
  canp_param_t* p = find_param_mut(full_name);
  if (p == NULL || p->type != CANP_TYPE_BOOL)
  {
    return false;
  }

  p->v.b = value ? 1U : 0U;
  p->valid = 1U;
  return true;
}

bool CanParams_SetInt32(const char* full_name, int32_t value)
{
  canp_param_t* p = find_param_mut(full_name);
  if (p == NULL || p->type != CANP_TYPE_INT32)
  {
    return false;
  }

  p->v.i32 = value;
  p->valid = 1U;
  return true;
}

bool CanParams_SetFloat(const char* full_name, float value)
{
  canp_param_t* p = find_param_mut(full_name);
  if (p == NULL || p->type != CANP_TYPE_FLOAT)
  {
    return false;
  }

  p->v.f = value;
  p->valid = 1U;
  return true;
}

/* RX-update entrypoints (raw values decoded from CAN frames) */
bool CanParams__UpdateBool(const char* full_name, uint8_t value)
{
  canp_param_t* p = find_param_mut(full_name);
  if (p == NULL || p->type != CANP_TYPE_BOOL)
  {
    return false;
  }

  p->v.b = (value != 0U) ? 1U : 0U;
  p->valid = 1U;
  return true;
}

bool CanParams__UpdateInt32(const char* full_name, int32_t value)
{
  canp_param_t* p = find_param_mut(full_name);
  if (p == NULL || p->type != CANP_TYPE_INT32)
  {
    return false;
  }

  p->v.i32 = value;
  p->valid = 1U;
  return true;
}

bool CanParams__UpdateFloat(const char* full_name, float value)
{
  canp_param_t* p = find_param_mut(full_name);
  if (p == NULL || p->type != CANP_TYPE_FLOAT)
  {
    return false;
  }

  p->v.f = value;
  p->valid = 1U;
  return true;
}
