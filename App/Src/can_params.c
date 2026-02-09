#include "can_params.h"

#include <string.h>
#include <stddef.h>

#include "stm32l4xx_hal.h" /* __disable_irq / __enable_irq / __get_PRIMASK */

/* =========================
 * Internal parameter store
 * ========================= */

#define CANP_MAX_PARAMS      (256U)
#define CANP_NAME_MAX        (64U)

/* Hash table size should be a power of two. */
#define CANP_HASH_SIZE       (512U)
#define CANP_HASH_MAX_PROBE  (32U)

#define CANP_INDEX_INVALID   (0xFFFFU)

typedef struct
{
  char name[CANP_NAME_MAX];
  canp_type_t type;

  /* All created params are considered initialized and valid.
   * Kept for compatibility with older code paths.
   */
  volatile uint8_t valid;

  /* For Bool/Int32/Float params, this links to the associated Event param.
   * For Event params, this links to itself.
   */
  uint16_t event_link;

  union
  {
    volatile uint8_t b;
    volatile int32_t i32;
    volatile float f;
  } v;
} canp_param_t;

static canp_param_t s_params[CANP_MAX_PARAMS];
static size_t s_param_count = 0;

/* Hash map from name -> param index.
 * Open addressing, linear probing.
 */
static int16_t s_hash[CANP_HASH_SIZE];

/* =========================
 * Utilities
 * ========================= */

static uint32_t fnv1a_32(const char* s)
{
  /* FNV-1a 32-bit */
  uint32_t h = 2166136261u;
  if (s == NULL)
    return h;
  for (const unsigned char* p = (const unsigned char*)s; *p != 0; p++)
  {
    h ^= (uint32_t)(*p);
    h *= 16777619u;
  }
  return h;
}

static void hash_reset(void)
{
  for (size_t i = 0; i < CANP_HASH_SIZE; i++)
  {
    s_hash[i] = -1;
  }
}

static bool hash_insert(const char* name, uint16_t index)
{
  if (name == NULL)
    return false;

  uint32_t h = fnv1a_32(name);
  uint32_t mask = (CANP_HASH_SIZE - 1U);
  uint32_t slot = h & mask;

  for (uint32_t probe = 0; probe < CANP_HASH_MAX_PROBE; probe++)
  {
    int16_t cur = s_hash[slot];
    if (cur < 0)
    {
      s_hash[slot] = (int16_t)index;
      return true;
    }

    /* Already there? */
    if (strncmp(s_params[(uint16_t)cur].name, name, CANP_NAME_MAX) == 0)
    {
      return true;
    }

    slot = (slot + 1U) & mask;
  }

  /* Table too full / too many collisions.
   * We allow correct behavior via linear search fallback.
   */
  return false;
}

static int find_param_index_fast(const char* full_name)
{
  if (full_name == NULL)
    return -1;

  uint32_t h = fnv1a_32(full_name);
  uint32_t mask = (CANP_HASH_SIZE - 1U);
  uint32_t slot = h & mask;

  for (uint32_t probe = 0; probe < CANP_HASH_MAX_PROBE; probe++)
  {
    int16_t idx = s_hash[slot];
    if (idx < 0)
    {
      return -1;
    }

    if (strncmp(s_params[(uint16_t)idx].name, full_name, CANP_NAME_MAX) == 0)
    {
      return (int)idx;
    }

    slot = (slot + 1U) & mask;
  }

  return -1;
}

static int find_param_index_linear(const char* full_name)
{
  if (full_name == NULL)
    return -1;

  for (size_t i = 0; i < s_param_count; i++)
  {
    if (strncmp(s_params[i].name, full_name, CANP_NAME_MAX) == 0)
    {
      return (int)i;
    }
  }
  return -1;
}

static int find_param_index(const char* full_name)
{
  int idx = find_param_index_fast(full_name);
  if (idx >= 0)
    return idx;
  return find_param_index_linear(full_name);
}

static void trim_name_inplace(char* s)
{
  if (s == NULL)
    return;

  /* Trim leading */
  size_t start = 0;
  while (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n')
    start++;

  if (start > 0)
  {
    size_t i = 0;
    while (s[start + i] != '\0')
    {
      s[i] = s[start + i];
      i++;
    }
    s[i] = '\0';
  }

  /* Trim trailing */
  size_t len = strlen(s);
  while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n'))
  {
    s[len - 1] = '\0';
    len--;
  }
}

static bool name_has_internal_space(const char* s)
{
  if (s == NULL)
    return false;
  for (const char* p = s; *p != '\0'; p++)
  {
    if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
      return true;
  }
  return false;
}

static bool normalize_and_validate_name(const char* in, char out[CANP_NAME_MAX])
{
  if (in == NULL || out == NULL)
    return false;

  (void)strncpy(out, in, CANP_NAME_MAX - 1U);
  out[CANP_NAME_MAX - 1U] = '\0';
  trim_name_inplace(out);

  if (out[0] == '\0')
    return false;

  /* Reject any embedded whitespace */
  if (name_has_internal_space(out))
    return false;

  /* Reject malformed dot forms like "MESSAGE." or ".SIGNAL" or "A..B" */
  const char* dot = strchr(out, '.');
  if (dot != NULL)
  {
    if (dot == out)
      return false;
    if (dot[1] == '\0')
      return false;
    if (strchr(dot + 1, '.') != NULL)
      return false;
  }

  return true;
}

static uint32_t enter_critical(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

static void exit_critical(uint32_t primask)
{
  if (primask == 0U)
  {
    __enable_irq();
  }
}

/* =========================
 * Internal API
 * ========================= */

void CanParams__Reset(void)
{
  for (size_t i = 0; i < CANP_MAX_PARAMS; i++)
  {
    s_params[i].name[0] = '\0';
    s_params[i].type = CANP_TYPE_INT32;
    s_params[i].valid = 0U;
    s_params[i].event_link = CANP_INDEX_INVALID;
    s_params[i].v.i32 = 0;
  }
  s_param_count = 0;
  hash_reset();
}

bool CanParams__Create(const char* full_name, canp_type_t type)
{
  char name_norm[CANP_NAME_MAX];
  if (!normalize_and_validate_name(full_name, name_norm))
  {
    return false;
  }

  /* Already exists? */
  int existing = find_param_index(name_norm);
  if (existing >= 0)
  {
    /* Keep original type; do not fail. */
    return true;
  }

  if (s_param_count >= CANP_MAX_PARAMS)
  {
    return false;
  }

  canp_param_t* p = &s_params[s_param_count];
  (void)memset(p, 0, sizeof(*p));
  (void)strncpy(p->name, name_norm, CANP_NAME_MAX - 1U);
  p->name[CANP_NAME_MAX - 1U] = '\0';
  p->type = type;
  p->valid = 1U;              /* initialized at creation */
  p->event_link = CANP_INDEX_INVALID;

  /* Defaults: zero/false */
  p->v.i32 = 0;
  p->v.b = 0U;

  /* For Event params, self-link by default */
  if (type == CANP_TYPE_EVENT)
  {
    p->event_link = (uint16_t)s_param_count;
  }

  (void)hash_insert(p->name, (uint16_t)s_param_count);
  s_param_count++;
  return true;
}

bool CanParams__LinkEvent(const char* param_full_name, const char* event_full_name)
{
  char param_norm[CANP_NAME_MAX];
  char event_norm[CANP_NAME_MAX];
  if (!normalize_and_validate_name(param_full_name, param_norm))
    return false;
  if (!normalize_and_validate_name(event_full_name, event_norm))
    return false;

  int pi = find_param_index(param_norm);
  int ei = find_param_index(event_norm);
  if (pi < 0 || ei < 0)
    return false;

  if (s_params[(size_t)ei].type != CANP_TYPE_EVENT)
    return false;

  if (s_params[(size_t)pi].type == CANP_TYPE_EVENT)
  {
    /* Event params link to self; do not override. */
    return true;
  }

  s_params[(size_t)pi].event_link = (uint16_t)ei;
  return true;
}

/* =========================
 * Public getters/setters
 * ========================= */

bool CanParams_GetBool(const char* full_name, bool* out_value)
{
  if (out_value == NULL)
    return false;

  char name_norm[CANP_NAME_MAX];
  if (!normalize_and_validate_name(full_name, name_norm))
    return false;

  int idx = find_param_index(name_norm);
  if (idx < 0)
    return false;

  const canp_param_t* p = &s_params[(size_t)idx];
  if (p->type != CANP_TYPE_BOOL)
    return false;

  *out_value = (p->v.b != 0U);
  return true;
}

bool CanParams_IsValid(const char* full_name)
{
  /* Deprecated: now means "exists" (all created params are initialized). */
  char name_norm[CANP_NAME_MAX];
  if (!normalize_and_validate_name(full_name, name_norm))
    return false;

  return (find_param_index(name_norm) >= 0);
}

bool CanParams_GetInt32(const char* full_name, int32_t* out_value)
{
  if (out_value == NULL)
    return false;

  char name_norm[CANP_NAME_MAX];
  if (!normalize_and_validate_name(full_name, name_norm))
    return false;

  int idx = find_param_index(name_norm);
  if (idx < 0)
    return false;

  const canp_param_t* p = &s_params[(size_t)idx];
  if (p->type != CANP_TYPE_INT32)
    return false;

  *out_value = p->v.i32;
  return true;
}

bool CanParams_GetFloat(const char* full_name, float* out_value)
{
  if (out_value == NULL)
    return false;

  char name_norm[CANP_NAME_MAX];
  if (!normalize_and_validate_name(full_name, name_norm))
    return false;

  int idx = find_param_index(name_norm);
  if (idx < 0)
    return false;

  const canp_param_t* p = &s_params[(size_t)idx];
  if (p->type != CANP_TYPE_FLOAT)
    return false;

  *out_value = p->v.f;
  return true;
}

bool CanParams_SetBool(const char* full_name, bool value)
{
  char name_norm[CANP_NAME_MAX];
  if (!normalize_and_validate_name(full_name, name_norm))
    return false;

  int idx = find_param_index(name_norm);
  if (idx < 0)
    return false;

  canp_param_t* p = &s_params[(size_t)idx];
  if (p->type != CANP_TYPE_BOOL)
    return false;

  p->v.b = value ? 1U : 0U;
  p->valid = 1U;
  return true;
}

bool CanParams_SetInt32(const char* full_name, int32_t value)
{
  char name_norm[CANP_NAME_MAX];
  if (!normalize_and_validate_name(full_name, name_norm))
    return false;

  int idx = find_param_index(name_norm);
  if (idx < 0)
    return false;

  canp_param_t* p = &s_params[(size_t)idx];
  if (p->type != CANP_TYPE_INT32)
    return false;

  p->v.i32 = value;
  p->valid = 1U;
  return true;
}

bool CanParams_SetFloat(const char* full_name, float value)
{
  char name_norm[CANP_NAME_MAX];
  if (!normalize_and_validate_name(full_name, name_norm))
    return false;

  int idx = find_param_index(name_norm);
  if (idx < 0)
    return false;

  canp_param_t* p = &s_params[(size_t)idx];
  if (p->type != CANP_TYPE_FLOAT)
    return false;

  p->v.f = value;
  p->valid = 1U;
  return true;
}

bool CanParams_GetEvent(const char* full_name, bool* out_event)
{
  if (out_event == NULL)
    return false;

  char name_norm[CANP_NAME_MAX];
  if (!normalize_and_validate_name(full_name, name_norm))
    return false;

  int idx = find_param_index(name_norm);
  if (idx < 0)
    return false;

  const canp_param_t* p = &s_params[(size_t)idx];

  uint16_t ei = (p->type == CANP_TYPE_EVENT) ? (uint16_t)idx : p->event_link;
  if (ei == CANP_INDEX_INVALID || ei >= (uint16_t)s_param_count)
    return false;

  const canp_param_t* ep = &s_params[(size_t)ei];
  if (ep->type != CANP_TYPE_EVENT)
    return false;

  *out_event = (ep->v.b != 0U);
  return true;
}

bool CanParams_ProcEvent(const char* full_name, bool* out_event)
{
  if (out_event == NULL)
    return false;

  char name_norm[CANP_NAME_MAX];
  if (!normalize_and_validate_name(full_name, name_norm))
    return false;

  int idx = find_param_index(name_norm);
  if (idx < 0)
    return false;

  canp_param_t* p = &s_params[(size_t)idx];
  uint16_t ei = (p->type == CANP_TYPE_EVENT) ? (uint16_t)idx : p->event_link;
  if (ei == CANP_INDEX_INVALID || ei >= (uint16_t)s_param_count)
    return false;

  canp_param_t* ep = &s_params[(size_t)ei];
  if (ep->type != CANP_TYPE_EVENT)
    return false;

  uint32_t primask = enter_critical();
  bool cur = (ep->v.b != 0U);
  if (cur)
  {
    ep->v.b = 0U;
  }
  exit_critical(primask);

  *out_event = cur;
  return true;
}

bool CanParams_SetEvent(const char* full_name, bool event_value)
{
  char name_norm[CANP_NAME_MAX];
  if (!normalize_and_validate_name(full_name, name_norm))
    return false;

  int idx = find_param_index(name_norm);
  if (idx < 0)
    return false;

  canp_param_t* p = &s_params[(size_t)idx];
  if (p->type != CANP_TYPE_EVENT)
    return false;

  p->v.b = event_value ? 1U : 0U;
  p->valid = 1U;
  return true;
}

/* =========================
 * Internal RX update helpers
 * ========================= */

bool CanParams__UpdateBool(const char* full_name, uint8_t value)
{
  return CanParams_SetBool(full_name, (value != 0U));
}

bool CanParams__UpdateInt32(const char* full_name, int32_t value)
{
  return CanParams_SetInt32(full_name, value);
}

bool CanParams__UpdateFloat(const char* full_name, float value)
{
  return CanParams_SetFloat(full_name, value);
}

bool CanParams__UpdateEvent(const char* full_name, bool event_value)
{
  return CanParams_SetEvent(full_name, event_value);
}
