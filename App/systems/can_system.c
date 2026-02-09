#include "can_system.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h> /* strtoul/strtof/strtol */
#include <stdio.h>  /* snprintf */

#include "can.h"
#include "main.h"

#include "can_params.h"
#include "can_config.h"

/* DBC text blob (generated from App/dbc/can_dbc_text.c) */
extern const char* g_can_dbc_text;

/* =========================
 *  DBC parsing (BO_/SG_ only)
 * ========================= */

typedef struct
{
  uint32_t msg_id;
  uint8_t dlc;
  char name[32];

  int16_t mux_sig_index; /* index into s_sigs, -1 if none */

  /* Non-mux: one event param named exactly message name.
   * Mux: one event param per mux value, named "<MSG>.__event_mux_<val>".
   */
  uint8_t has_data; /* 1 if any signal has bit_length > 0 */

  /* Per-message debug (non-mux only). For mux messages, per-page debug is used. */
  uint32_t last_rx_tick;
  uint32_t last_tx_tick;

  /* Mux message: mux values defined in DBC (no "always" signals exist per project rules).
   * Arrays are compact and avoid per-page name storage to keep RAM low.
   */
  uint8_t mux_vals[32];
  uint8_t mux_has_data[32];
  uint32_t mux_last_rx_tick[32];
  uint32_t mux_last_tx_tick[32];
  uint8_t page_count;
} dbc_msg_t;

typedef struct
{
  char full_name[64]; /* "MESSAGE.SIGNAL" */
  uint16_t msg_index; /* index into s_msgs */

  uint16_t start_bit;
  uint16_t length;
  bool is_signed;
  float factor;
  float offset;
  canp_type_t type;

  /* Multiplexing */
  uint8_t is_mux;  /* this signal is the multiplexor ("M") */
  int16_t mux_val; /* -1 if not muxed, else required mux value ("m17M") */
} dbc_sig_t;

#define MAX_DBC_MSGS  (64U)
#define MAX_DBC_SIGS  (256U)

static dbc_msg_t s_msgs[MAX_DBC_MSGS];
static dbc_sig_t s_sigs[MAX_DBC_SIGS];
static uint32_t s_msg_count = 0;
static uint32_t s_sig_count = 0;

/* TX pending queue:
 * - Non-mux messages: mux_val = 0xFF sentinel
 * - Mux messages: pending mux values (0..255)
 */
#define MAX_PENDING_MUX (8U)
typedef struct
{
  uint8_t pending_mux_vals[MAX_PENDING_MUX];
  uint8_t pending_mux_count;
} tx_pending_t;

static tx_pending_t s_tx_pending[MAX_DBC_MSGS];

/* Deprecated inbox/outbox flags (backed by params "pending_inbox"/"pending_outbox") */
static uint8_t s_inbox_updated_since_tick = 0U;

/* =========================
 *  Allow list (used for TX+RX include)
 * ========================= */

static bool id_in_allow_list(uint32_t std_id)
{
  if (g_can_rx_id_filter_count == 0U)
  {
    return true;
  }

  for (size_t i = 0; i < g_can_rx_id_filter_count; i++)
  {
    if ((uint32_t)g_can_rx_id_filter[i] == std_id)
    {
      return true;
    }
  }

  return false;
}

/* =========================
 *  Hardware filter setup
 * ========================= */

static void can_apply_filters(void)
{
  CAN_FilterTypeDef f;
  (void)memset(&f, 0, sizeof(f));

  f.FilterActivation = ENABLE;
  f.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  f.SlaveStartFilterBank = 14;

  if (g_can_rx_id_filter_count == 0U)
  {
    /* Accept ALL standard IDs */
    f.FilterBank = 0;
    f.FilterMode = CAN_FILTERMODE_IDMASK;
    f.FilterScale = CAN_FILTERSCALE_32BIT;

    f.FilterIdHigh = 0x0000;
    f.FilterIdLow  = 0x0000;
    f.FilterMaskIdHigh = 0x0000;
    f.FilterMaskIdLow  = 0x0000;

    (void)HAL_CAN_ConfigFilter(&hcan1, &f);
    return;
  }

  uint32_t bank = 0;
  size_t i = 0;

  while (i < g_can_rx_id_filter_count)
  {
    if (bank >= 14U)
    {
      /* remainder still filtered in software */
      break;
    }

    uint16_t id1 = (uint16_t)((uint32_t)g_can_rx_id_filter[i] & 0x7FFU);
    uint16_t id2 = id1;
    if ((i + 1U) < g_can_rx_id_filter_count)
    {
      id2 = (uint16_t)((uint32_t)g_can_rx_id_filter[i + 1U] & 0x7FFU);
    }

    f.FilterBank = bank;
    f.FilterMode = CAN_FILTERMODE_IDLIST;
    f.FilterScale = CAN_FILTERSCALE_32BIT;

    /* Correct HAL layout for StdId in 32-bit mode: High=(ID<<5), Low=0 */
    f.FilterIdHigh     = (uint16_t)(id1 << 5);
    f.FilterIdLow      = 0x0000;
    f.FilterMaskIdHigh = (uint16_t)(id2 << 5);
    f.FilterMaskIdLow  = 0x0000;

    (void)HAL_CAN_ConfigFilter(&hcan1, &f);

    bank++;
    i += 2U;
  }
}

/* =========================
 *  Helpers
 * ========================= */

static void tx_pending_clear(uint32_t mi)
{
  if (mi < MAX_DBC_MSGS)
  {
    s_tx_pending[mi].pending_mux_count = 0U;
  }
}

static bool tx_pending_add(uint32_t mi, uint8_t mux_val)
{
  if (mi >= s_msg_count)
    return false;

  tx_pending_t* p = &s_tx_pending[mi];
  for (uint8_t i = 0; i < p->pending_mux_count; i++)
  {
    if (p->pending_mux_vals[i] == mux_val)
    {
      return true; /* idempotent */
    }
  }

  if (p->pending_mux_count >= MAX_PENDING_MUX)
  {
    return false;
  }

  p->pending_mux_vals[p->pending_mux_count++] = mux_val;
  return true;
}

static bool tx_any_pending(void)
{
  for (uint32_t mi = 0; mi < s_msg_count; mi++)
  {
    if (s_tx_pending[mi].pending_mux_count != 0U)
      return true;
  }
  return false;
}

static int find_msg_index_by_id(uint32_t msg_id)
{
  for (uint32_t i = 0; i < s_msg_count; i++)
  {
    if (s_msgs[i].msg_id == msg_id)
    {
      return (int)i;
    }
  }
  return -1;
}

static int find_msg_index_by_name(const char* msg_name)
{
  if (msg_name == NULL)
    return -1;

  for (uint32_t i = 0; i < s_msg_count; i++)
  {
    if (strcmp(s_msgs[i].name, msg_name) == 0)
    {
      return (int)i;
    }
  }
  return -1;
}

static int find_sig_index_by_full_name(const char* full_name)
{
  if (full_name == NULL)
    return -1;

  for (uint32_t i = 0; i < s_sig_count; i++)
  {
    if (strcmp(s_sigs[i].full_name, full_name) == 0)
    {
      return (int)i;
    }
  }
  return -1;
}

static bool build_full_name(char* dst, size_t dst_size, const char* msg_name, const char* sig_name)
{
  if (dst == NULL || dst_size == 0U || msg_name == NULL || sig_name == NULL)
  {
    return false;
  }

  dst[0] = '\0';
  size_t used = 0U;

  for (size_t i = 0U; msg_name[i] != '\0'; i++)
  {
    if (used + 1U >= dst_size)
    {
      dst[dst_size - 1U] = '\0';
      return false;
    }
    dst[used++] = msg_name[i];
  }

  if (used + 1U >= dst_size)
  {
    dst[dst_size - 1U] = '\0';
    return false;
  }
  dst[used++] = '.';

  for (size_t i = 0U; sig_name[i] != '\0'; i++)
  {
    if (used + 1U >= dst_size)
    {
      dst[dst_size - 1U] = '\0';
      return false;
    }
    dst[used++] = sig_name[i];
  }

  dst[used] = '\0';
  return true;
}

static void trim_ws_inplace(char* s)
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
  while (len > 0U)
  {
    char c = s[len - 1U];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
    {
      s[len - 1U] = '\0';
      len--;
    }
    else
    {
      break;
    }
  }
}

/* =========================
 *  Parsing utilities
 * ========================= */

static bool parse_uint(const char* s, uint32_t* out)
{
  if (s == NULL || out == NULL)
    return false;

  char* end = NULL;
  unsigned long v = strtoul(s, &end, 10);
  if (end == s)
    return false;
  *out = (uint32_t)v;
  return true;
}

static bool parse_float(const char* s, float* out)
{
  if (s == NULL || out == NULL)
    return false;

  char* end = NULL;
  float v = strtof(s, &end);
  if (end == s)
    return false;
  *out = v;
  return true;
}

/* Parse mux token from SG_ line:
 * - "M"      => multiplexor
 * - "m17M"   => muxed signal for mux=17
 * - "m1M"    => some DBCs use m1M with no leading zeros
 */
static void parse_mux_token(const char* tok, uint8_t* is_mux, int16_t* mux_val)
{
  *is_mux = 0U;
  *mux_val = -1;

  if (tok == NULL || tok[0] == '\0')
    return;

  if (strcmp(tok, "M") == 0)
  {
    *is_mux = 1U;
    *mux_val = -1;
    return;
  }

  if (tok[0] == 'm')
  {
    char* end = NULL;
    long v = strtol(tok + 1, &end, 10);
    if (end != NULL && *end == 'M' && v >= 0 && v <= 255)
    {
      *is_mux = 0U;
      *mux_val = (int16_t)v;
      return;
    }
  }
}

static bool msg_add_mux_page(uint32_t mi, uint8_t mux_val)
{
  dbc_msg_t* m = &s_msgs[mi];
  for (uint8_t i = 0; i < m->page_count; i++)
  {
    if (m->mux_vals[i] == mux_val)
      return true;
  }

  if (m->page_count >= (uint8_t)(sizeof(m->mux_vals) / sizeof(m->mux_vals[0])))
  {
    return false;
  }

  uint8_t idx = m->page_count++;
  m->mux_vals[idx] = mux_val;
  m->mux_has_data[idx] = 0U;
  m->mux_last_rx_tick[idx] = 0U;
  m->mux_last_tx_tick[idx] = 0U;
  return true;
}

static int msg_find_page_index(uint32_t mi, uint8_t mux_val)
{
  const dbc_msg_t* m = &s_msgs[mi];
  for (uint8_t i = 0; i < m->page_count; i++)
  {
    if (m->mux_vals[i] == mux_val)
      return (int)i;
  }
  return -1;
}

static void dbc_parse_reset(void)
{
  s_msg_count = 0U;
  s_sig_count = 0U;

  for (uint32_t i = 0; i < MAX_DBC_MSGS; i++)
  {
    (void)memset(&s_msgs[i], 0, sizeof(s_msgs[i]));
    s_msgs[i].mux_sig_index = -1;
    s_msgs[i].last_rx_tick = 0U;
    s_msgs[i].last_tx_tick = 0U;
    s_msgs[i].has_data = 0U;
    s_msgs[i].page_count = 0U;
    tx_pending_clear(i);
  }

  for (uint32_t i = 0; i < MAX_DBC_SIGS; i++)
  {
    (void)memset(&s_sigs[i], 0, sizeof(s_sigs[i]));
    s_sigs[i].msg_index = 0U;
    s_sigs[i].start_bit = 0U;
    s_sigs[i].length = 0U;
    s_sigs[i].is_signed = false;
    s_sigs[i].factor = 1.0f;
    s_sigs[i].offset = 0.0f;
    s_sigs[i].type = CANP_TYPE_INT32;
    s_sigs[i].is_mux = 0U;
    s_sigs[i].mux_val = -1;
  }
}

static bool dbc_finalize_event_params_and_links(void)
{
  /* Create event params + link every value param to its event param */

  for (uint32_t mi = 0; mi < s_msg_count; mi++)
  {
    dbc_msg_t* m = &s_msgs[mi];

    if (m->mux_sig_index < 0)
    {
      /* Non-mux: event param name is exactly the message name (so user may call CanParams_*Event("MESSAGE")) */
      if (!CanParams__Create(m->name, CANP_TYPE_EVENT))
        return false;

      /* Link all signals in this message */
      for (uint32_t si = 0; si < s_sig_count; si++)
      {
        if (s_sigs[si].msg_index != (uint16_t)mi)
          continue;
        (void)CanParams__LinkEvent(s_sigs[si].full_name, m->name);
      }
    }
    else
    {
      /* Muxed: create one event param per DBC-defined mux value.
       * Internal event name format: "<MSG>.__event_mux_<val>".
       */
      for (uint8_t pi = 0; pi < m->page_count; pi++)
      {
        char evn[64];
        int n = snprintf(evn, sizeof(evn), "%s.__event_mux_%u", m->name, (unsigned)m->mux_vals[pi]);
        if (n <= 0 || (size_t)n >= sizeof(evn))
          return false;
        if (!CanParams__Create(evn, CANP_TYPE_EVENT))
          return false;
      }

      /* Link each signal to its mux-page event. (Do not link mux selector.) */
      for (uint32_t si = 0; si < s_sig_count; si++)
      {
        if (s_sigs[si].msg_index != (uint16_t)mi)
          continue;
        if (s_sigs[si].is_mux != 0U)
          continue;
        if (s_sigs[si].mux_val < 0)
          continue;

        int pidx = msg_find_page_index(mi, (uint8_t)s_sigs[si].mux_val);
        if (pidx < 0)
          continue;

        char evn[64];
        int n = snprintf(evn, sizeof(evn), "%s.__event_mux_%u", m->name, (unsigned)m->mux_vals[(uint8_t)pidx]);
        if (n > 0 && (size_t)n < sizeof(evn))
        {
          (void)CanParams__LinkEvent(s_sigs[si].full_name, evn);
        }
      }
    }
  }

  return true;
}

static bool dbc_parse_all(void)
{
  dbc_parse_reset();

  CanParams__Reset();

  /* Deprecated globals */
  (void)CanParams__Create("pending_inbox", CANP_TYPE_BOOL);
  (void)CanParams__Create("pending_outbox", CANP_TYPE_BOOL);
  (void)CanParams_SetBool("pending_inbox", false);
  (void)CanParams_SetBool("pending_outbox", false);

  if (g_can_dbc_text == NULL)
  {
    return false;
  }

  uint32_t current_msg_id = 0U;
  uint32_t current_msg_dlc = 8U;
  char current_msg_name[32] = {0};
  bool current_included = false;
  int current_mi = -1;

  const char* p = g_can_dbc_text;
  while (*p != '\0')
  {
    char line[256];
    size_t li = 0;
    while (*p != '\0' && *p != '\n' && li < (sizeof(line) - 1U))
    {
      line[li++] = *p++;
    }
    if (*p == '\n')
      p++;
    line[li] = '\0';

    /* Trim leading whitespace */
    const char* lp = line;
    while (*lp == ' ' || *lp == '\t')
      lp++;

    /* Only BO_ / SG_ are recognized */
    if (strncmp(lp, "BO_ ", 4) == 0)
    {
      /* BO_ <id> <name>: <dlc> ... */
      char tmp[256];
      (void)strncpy(tmp, lp + 4, sizeof(tmp) - 1U);
      tmp[sizeof(tmp) - 1U] = '\0';
      trim_ws_inplace(tmp);

      /* Extract id */
      char* s_id = tmp;
      char* sp = strchr(s_id, ' ');
      if (sp == NULL)
        continue;
      *sp = '\0';
      sp++;
      while (*sp == ' ') sp++;

      uint32_t msg_id = 0U;
      if (!parse_uint(s_id, &msg_id))
        continue;

      /* Extract name (up to ':') */
      char* colon = strchr(sp, ':');
      if (colon == NULL)
        continue;
      *colon = '\0';
      trim_ws_inplace(sp);
      if (sp[0] == '\0')
        continue;

      (void)strncpy(current_msg_name, sp, sizeof(current_msg_name) - 1U);
      current_msg_name[sizeof(current_msg_name) - 1U] = '\0';
      current_msg_id = msg_id;

      /* DLC is after ':' */
      char* after_colon = colon + 1;
      while (*after_colon == ' ') after_colon++;
      char* end_dlc = strchr(after_colon, ' ');
      if (end_dlc != NULL)
        *end_dlc = '\0';

      uint32_t dlc = 8U;
      (void)parse_uint(after_colon, &dlc);
      if (dlc > 8U) dlc = 8U;

      current_msg_dlc = dlc;
      current_included = id_in_allow_list(current_msg_id);
      current_mi = -1;

      if (!current_included)
      {
        continue;
      }

      if (s_msg_count >= MAX_DBC_MSGS)
      {
        return false;
      }

      dbc_msg_t* m = &s_msgs[s_msg_count];
      (void)memset(m, 0, sizeof(*m));
      m->msg_id = current_msg_id;
      m->dlc = (uint8_t)current_msg_dlc;
      (void)strncpy(m->name, current_msg_name, sizeof(m->name) - 1U);
      m->name[sizeof(m->name) - 1U] = '\0';
      m->mux_sig_index = -1;
      m->has_data = 0U;
      m->last_rx_tick = 0U;
      m->last_tx_tick = 0U;
      m->page_count = 0U;

      tx_pending_clear(s_msg_count);

      current_mi = (int)s_msg_count;
      s_msg_count++;
    }
    else if (strncmp(lp, "SG_ ", 4) == 0)
    {
      if (!current_included || current_mi < 0)
      {
        continue;
      }

      /* SG_ <sig> [mux] : <start>|<len>@<endian><sign> (<factor>,<offset>) ... */
      const char* sg = lp + 4;
      const char* colon = strchr(sg, ':');
      if (colon == NULL)
        continue;

      char left[128];
      size_t ln = (size_t)(colon - sg);
      if (ln >= sizeof(left))
        continue;
      (void)memcpy(left, sg, ln);
      left[ln] = '\0';
      trim_ws_inplace(left);
      if (left[0] == '\0')
        continue;

      /* Tokenize left: <sig_name> [mux_token] */
      char* sig_name = strtok(left, " \t");
      if (sig_name == NULL)
        continue;
      char* mux_tok = strtok(NULL, " \t");

      uint8_t is_mux = 0U;
      int16_t mux_val = -1;
      if (mux_tok != NULL)
      {
        parse_mux_token(mux_tok, &is_mux, &mux_val);
      }

      const char* rhs = colon + 1;
      while (*rhs == ' ') rhs++;

      /* Parse start|len */
      const char* bar = strchr(rhs, '|');
      const char* at = strchr(rhs, '@');
      if (bar == NULL || at == NULL || bar > at)
        continue;

      char tmpnum[16];
      (void)memset(tmpnum, 0, sizeof(tmpnum));

      size_t n = (size_t)(bar - rhs);
      if (n == 0 || n >= sizeof(tmpnum))
        continue;
      (void)memcpy(tmpnum, rhs, n);
      tmpnum[n] = '\0';
      uint32_t start_bit = 0U;
      if (!parse_uint(tmpnum, &start_bit))
        continue;

      n = (size_t)(at - (bar + 1));
      if (n == 0 || n >= sizeof(tmpnum))
        continue;
      (void)memcpy(tmpnum, bar + 1, n);
      tmpnum[n] = '\0';
      uint32_t bitlen = 0U;
      if (!parse_uint(tmpnum, &bitlen))
        continue;

      /* Signedness: at[2] is '+' or '-' in common DBC formats */
      bool is_signed_sig = false;
      if (at[2] == '-')
        is_signed_sig = true;

      /* Parse (factor,offset) if present */
      float factor = 1.0f;
      float offset = 0.0f;
      const char* lp2 = strchr(rhs, '(');
      const char* rp2 = strchr(rhs, ')');
      if (lp2 != NULL && rp2 != NULL && rp2 > lp2)
      {
        char pair[64];
        size_t pn = (size_t)(rp2 - (lp2 + 1));
        if (pn < sizeof(pair))
        {
          (void)memcpy(pair, lp2 + 1, pn);
          pair[pn] = '\0';
          char* comma = strchr(pair, ',');
          if (comma != NULL)
          {
            *comma = '\0';
            (void)parse_float(pair, &factor);
            (void)parse_float(comma + 1, &offset);
          }
        }
      }

      canp_type_t type = CANP_TYPE_INT32;
      if (factor != 1.0f || offset != 0.0f)
        type = CANP_TYPE_FLOAT;
      else if (bitlen == 1U)
        type = CANP_TYPE_BOOL;
      else
        type = CANP_TYPE_INT32;

      if (s_sig_count >= MAX_DBC_SIGS)
      {
        return false;
      }

      dbc_sig_t* s = &s_sigs[s_sig_count];
      (void)memset(s, 0, sizeof(*s));

      if (!build_full_name(s->full_name, sizeof(s->full_name), current_msg_name, sig_name))
      {
        return false;
      }

      s->msg_index = (uint16_t)current_mi;
      s->start_bit = (uint16_t)start_bit;
      s->length = (uint16_t)bitlen;
      s->is_signed = is_signed_sig;
      s->factor = factor;
      s->offset = offset;
      s->type = type;
      s->is_mux = is_mux;
      s->mux_val = mux_val;

      /* Create DB param for the signal */
      if (!CanParams__Create(s->full_name, type))
      {
        return false;
      }

      /* Message bookkeeping */
      dbc_msg_t* m = &s_msgs[(uint32_t)current_mi];

      if (bitlen > 0U)
      {
        m->has_data = 1U;
      }

      if (is_mux != 0U)
      {
        m->mux_sig_index = (int16_t)s_sig_count;
      }

      if (mux_val >= 0)
      {
        if (!msg_add_mux_page((uint32_t)current_mi, (uint8_t)mux_val))
        {
          return false;
        }
        int pidx = msg_find_page_index((uint32_t)current_mi, (uint8_t)mux_val);
        if (pidx >= 0 && bitlen > 0U)
        {
          m->mux_has_data[(uint8_t)pidx] = 1U;
        }
      }

      s_sig_count++;
    }
  }

  /* Create event params and link signal->event mapping */
  if (!dbc_finalize_event_params_and_links())
  {
    return false;
  }

  return true;
}

/* =========================
 *  Bit helpers (little-endian)
 * ========================= */

static uint32_t extract_bits_le(const uint8_t* data, uint16_t start_bit, uint16_t length)
{
  if (length == 0U)
    return 0U;

  uint32_t out = 0U;
  for (uint16_t i = 0; i < length && i < 32U; i++)
  {
    uint16_t bit_index = start_bit + i;
    uint16_t byte_index = bit_index / 8U;
    uint8_t bit_in_byte = (uint8_t)(bit_index % 8U);
    uint8_t bit = (uint8_t)((data[byte_index] >> bit_in_byte) & 0x1U);
    out |= ((uint32_t)bit << i);
  }
  return out;
}

static int32_t sign_extend(uint32_t raw, uint16_t bitlen)
{
  if (bitlen == 0U || bitlen >= 32U)
    return (int32_t)raw;

  uint32_t sign_bit = 1UL << (bitlen - 1U);
  if ((raw & sign_bit) != 0U)
  {
    uint32_t mask = ~((1UL << bitlen) - 1U);
    raw |= mask;
  }
  return (int32_t)raw;
}

static void insert_bits_le(uint8_t* data, uint16_t start_bit, uint16_t length, uint32_t value)
{
  if (length == 0U)
    return;

  for (uint16_t i = 0; i < length && i < 32U; i++)
  {
    uint16_t bit_index = start_bit + i;
    uint16_t byte_index = bit_index / 8U;
    uint8_t bit_in_byte = (uint8_t)(bit_index % 8U);

    uint8_t bit = (uint8_t)((value >> i) & 0x1U);
    data[byte_index] &= (uint8_t)~(1U << bit_in_byte);
    data[byte_index] |= (uint8_t)(bit << bit_in_byte);
  }
}

static uint32_t mask_to_length(uint32_t v, uint16_t bitlen)
{
  if (bitlen == 0U)
    return 0U;
  if (bitlen >= 32U)
    return v;
  return (v & ((1UL << bitlen) - 1U));
}

/* =========================
 *  RX decode + FIFO drain
 * ========================= */

static void set_event_nonmux(uint32_t mi)
{
  dbc_msg_t* m = &s_msgs[mi];
  /* Non-mux: event param name is exactly the message name */
  (void)CanParams__UpdateEvent(m->name, true);
  m->last_rx_tick = HAL_GetTick();
}

static void set_event_mux(uint32_t mi, uint8_t mux_val)
{
  dbc_msg_t* m = &s_msgs[mi];
  int pidx = msg_find_page_index(mi, mux_val);
  if (pidx < 0)
    return;
  char evn[64];
  int n = snprintf(evn, sizeof(evn), "%s.__event_mux_%u", m->name, (unsigned)mux_val);
  if (n <= 0 || (size_t)n >= sizeof(evn))
    return;
  (void)CanParams__UpdateEvent(evn, true);
  m->mux_last_rx_tick[(uint8_t)pidx] = HAL_GetTick();
}

static void handle_rx_frame(uint32_t std_id, const uint8_t* data, uint8_t dlc)
{
  (void)dlc;

  if (!id_in_allow_list(std_id))
    return;

  int mi = find_msg_index_by_id(std_id);
  if (mi < 0)
    return;

  dbc_msg_t* m = &s_msgs[(uint32_t)mi];
  bool wrote_any_data = false;

  if (m->mux_sig_index < 0)
  {
    /* Non-mux message */
    for (uint32_t si = 0; si < s_sig_count; si++)
    {
      const dbc_sig_t* s = &s_sigs[si];
      if (s->msg_index != (uint16_t)mi)
        continue;

      uint32_t raw = extract_bits_le(data, s->start_bit, s->length);
      bool updated = false;

      if (s->type == CANP_TYPE_BOOL)
      {
        updated = CanParams__UpdateBool(s->full_name, (uint8_t)(raw != 0U));
      }
      else if (s->type == CANP_TYPE_FLOAT)
      {
        int32_t signed_raw = s->is_signed ? sign_extend(raw, s->length) : (int32_t)raw;
        float phys = (s->factor == 0.0f) ? s->offset : (((float)signed_raw) * s->factor + s->offset);
        updated = CanParams__UpdateFloat(s->full_name, phys);
      }
      else
      {
        int32_t signed_raw = s->is_signed ? sign_extend(raw, s->length) : (int32_t)raw;
        updated = CanParams__UpdateInt32(s->full_name, signed_raw);
      }

      if (updated)
      {
        s_inbox_updated_since_tick = 1U;
        if (s->length > 0U)
          wrote_any_data = true;
      }
    }

    /* Event latch timing rule */
    if (m->has_data == 0U)
    {
      /* No-data message: set after recognition */
      set_event_nonmux((uint32_t)mi);
      s_inbox_updated_since_tick = 1U;
    }
    else if (wrote_any_data)
    {
      set_event_nonmux((uint32_t)mi);
      s_inbox_updated_since_tick = 1U;
    }

    return;
  }

  /* Muxed message */
  const dbc_sig_t* mux_sig = &s_sigs[(uint32_t)m->mux_sig_index];
  uint32_t mux_raw = extract_bits_le(data, mux_sig->start_bit, mux_sig->length);
  uint8_t mux_val = (uint8_t)(mux_raw & 0xFFU);

  /* Ignore undefined mux values */
  int pidx = msg_find_page_index((uint32_t)mi, mux_val);
  if (pidx < 0)
    return;

  /* Update mux selector param (does not count toward wrote_any_data) */
  if (mux_sig->type == CANP_TYPE_BOOL)
  {
    if (CanParams__UpdateBool(mux_sig->full_name, (uint8_t)(mux_raw != 0U)))
      s_inbox_updated_since_tick = 1U;
  }
  else
  {
    if (CanParams__UpdateInt32(mux_sig->full_name, (int32_t)mux_raw))
      s_inbox_updated_since_tick = 1U;
  }

  /* Decode only signals in this mux page */
  for (uint32_t si = 0; si < s_sig_count; si++)
  {
    const dbc_sig_t* s = &s_sigs[si];
    if (s->msg_index != (uint16_t)mi)
      continue;
    if (s->is_mux != 0U)
      continue;
    if (s->mux_val < 0)
      continue;
    if ((uint8_t)s->mux_val != mux_val)
      continue;

    uint32_t raw = extract_bits_le(data, s->start_bit, s->length);
    bool updated = false;

    if (s->type == CANP_TYPE_BOOL)
    {
      updated = CanParams__UpdateBool(s->full_name, (uint8_t)(raw != 0U));
    }
    else if (s->type == CANP_TYPE_FLOAT)
    {
      int32_t signed_raw = s->is_signed ? sign_extend(raw, s->length) : (int32_t)raw;
      float phys = (s->factor == 0.0f) ? s->offset : (((float)signed_raw) * s->factor + s->offset);
      updated = CanParams__UpdateFloat(s->full_name, phys);
    }
    else
    {
      int32_t signed_raw = s->is_signed ? sign_extend(raw, s->length) : (int32_t)raw;
      updated = CanParams__UpdateInt32(s->full_name, signed_raw);
    }

    if (updated)
    {
      s_inbox_updated_since_tick = 1U;
      if (s->length > 0U)
        wrote_any_data = true;
    }
  }

  /* Event latch timing rule */
  if (m->mux_has_data[(uint8_t)pidx] == 0U)
  {
    set_event_mux((uint32_t)mi, mux_val);
    s_inbox_updated_since_tick = 1U;
  }
  else if (wrote_any_data)
  {
    set_event_mux((uint32_t)mi, mux_val);
    s_inbox_updated_since_tick = 1U;
  }
}

static void process_rx_fifo0(void)
{
  CAN_RxHeaderTypeDef rx_header;
  uint8_t data[8];

  while (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) > 0U)
  {
    if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &rx_header, data) != HAL_OK)
    {
      return;
    }

    if (rx_header.IDE == CAN_ID_STD)
    {
      handle_rx_frame(rx_header.StdId, data, rx_header.DLC);
    }
  }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* hcan)
{
  if (hcan != &hcan1)
    return;
  process_rx_fifo0();
}

/* =========================
 *  TX encode
 * ========================= */

static bool encode_raw_from_param(const dbc_sig_t* s, uint32_t* out_raw)
{
  if (s == NULL || out_raw == NULL)
    return false;

  if (s->type == CANP_TYPE_BOOL)
  {
    bool bv = false;
    if (!CanParams_GetBool(s->full_name, &bv))
      bv = false;
    *out_raw = bv ? 1U : 0U;
    return true;
  }
  else if (s->type == CANP_TYPE_FLOAT)
  {
    float fv = 0.0f;
    if (!CanParams_GetFloat(s->full_name, &fv))
      fv = 0.0f;

    if (s->factor == 0.0f)
    {
      *out_raw = 0U;
      return true;
    }

    float fr = (fv - s->offset) / s->factor;
    int32_t ir = (int32_t)fr;
    *out_raw = (uint32_t)ir;
    return true;
  }
  else
  {
    int32_t iv = 0;
    if (!CanParams_GetInt32(s->full_name, &iv))
      iv = 0;
    *out_raw = (uint32_t)iv;
    return true;
  }
}

static bool transmit_one(uint32_t mi, uint8_t mux_to_send)
{
  if (mi >= s_msg_count)
    return false;

  uint8_t data[8] = {0};

  dbc_msg_t* m = &s_msgs[mi];
  if (m->mux_sig_index >= 0)
  {
    const dbc_sig_t* ms = &s_sigs[(uint32_t)m->mux_sig_index];
    insert_bits_le(data, ms->start_bit, ms->length, (uint32_t)mux_to_send);
  }

  for (uint32_t si = 0; si < s_sig_count; si++)
  {
    const dbc_sig_t* s = &s_sigs[si];
    if (s->msg_index != (uint16_t)mi)
      continue;

    if (m->mux_sig_index >= 0)
    {
      if (s->is_mux != 0U)
        continue;
      if (s->mux_val < 0)
        continue;
      if ((uint8_t)s->mux_val != mux_to_send)
        continue;
    }

    uint32_t raw = 0U;
    (void)encode_raw_from_param(s, &raw);
    raw = mask_to_length(raw, s->length);
    insert_bits_le(data, s->start_bit, s->length, raw);
  }

  CAN_TxHeaderTypeDef txh;
  txh.StdId = m->msg_id;
  txh.ExtId = 0U;
  txh.IDE = CAN_ID_STD;
  txh.RTR = CAN_RTR_DATA;
  txh.DLC = m->dlc;
  txh.TransmitGlobalTime = DISABLE;

  uint32_t mbx = 0U;
  if (HAL_CAN_AddTxMessage(&hcan1, &txh, data, &mbx) != HAL_OK)
  {
    return false;
  }

  /* Success */
  m->last_tx_tick = HAL_GetTick();
  if (m->mux_sig_index >= 0)
  {
    int pidx = msg_find_page_index(mi, mux_to_send);
    if (pidx >= 0)
    {
      m->mux_last_tx_tick[(uint8_t)pidx] = m->last_tx_tick;
    }
  }

  return true;
}

static void transmit_pending_messages(void)
{
  for (uint32_t mi = 0; mi < s_msg_count; mi++)
  {
    tx_pending_t* pend = &s_tx_pending[mi];
    if (pend->pending_mux_count == 0U)
      continue;

    dbc_msg_t* m = &s_msgs[mi];

    if (m->mux_sig_index < 0)
    {
      /* Non-mux: expect sentinel 0xFF */
      bool ok = transmit_one(mi, 0xFFU);
      if (ok)
      {
        tx_pending_clear(mi);
      }
      continue;
    }

    /* Muxed: send each pending mux page in insertion order.
     * Only clear entries that were successfully sent.
     */
    uint8_t new_vals[MAX_PENDING_MUX];
    uint8_t new_count = 0U;

    for (uint8_t i = 0; i < pend->pending_mux_count; i++)
    {
      uint8_t mux = pend->pending_mux_vals[i];
      bool ok = transmit_one(mi, mux);
      if (!ok)
      {
        if (new_count < MAX_PENDING_MUX)
        {
          new_vals[new_count++] = mux;
        }
      }
    }

    pend->pending_mux_count = new_count;
    for (uint8_t i = 0; i < new_count; i++)
    {
      pend->pending_mux_vals[i] = new_vals[i];
    }
  }

  (void)CanParams_SetBool("pending_outbox", tx_any_pending());
}

/* =========================
 *  Controller
 * ========================= */

void can_system_controller(void)
{
  static uint8_t s_inited = 0U;

  if (!s_inited)
  {
    if (!dbc_parse_all())
    {
      /* If parsing fails, leave CAN running but with no params. */
    }

    can_apply_filters();
    (void)HAL_CAN_Start(&hcan1);
    (void)HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);

    s_inited = 1U;
  }

  process_rx_fifo0();

  (void)CanParams_SetBool("pending_inbox", (s_inbox_updated_since_tick != 0U));
  s_inbox_updated_since_tick = 0U;

  if (tx_any_pending())
  {
    transmit_pending_messages();
  }
  else
  {
    (void)CanParams_SetBool("pending_outbox", false);
  }
}

/* =========================
 *  New external TX scheduling API
 * ========================= */

bool CanSystem_Send(const char* full_name)
{
  if (full_name == NULL)
    return false;

  /* Reject deprecated globals */
  if (strcmp(full_name, "pending_inbox") == 0 || strcmp(full_name, "pending_outbox") == 0)
    return false;

  /* Copy and trim */
  char name[64];
  (void)strncpy(name, full_name, sizeof(name) - 1U);
  name[sizeof(name) - 1U] = '\0';
  trim_ws_inplace(name);
  if (name[0] == '\0')
    return false;

  const char* dot = strchr(name, '.');
  if (dot == NULL)
  {
    /* "MESSAGE" form: only valid for non-mux messages */
    int mi = find_msg_index_by_name(name);
    if (mi < 0)
      return false;
    if (s_msgs[(uint32_t)mi].mux_sig_index >= 0)
      return false;

    if (!tx_pending_add((uint32_t)mi, 0xFFU))
      return false;

    (void)CanParams_SetBool("pending_outbox", true);
    return true;
  }

  /* "MESSAGE.SIGNAL" form */
  if (dot == name || dot[1] == '\0')
    return false;
  if (strchr(dot + 1, '.') != NULL)
    return false;

  int si = find_sig_index_by_full_name(name);
  if (si < 0)
    return false;

  uint32_t mi = (uint32_t)s_sigs[(uint32_t)si].msg_index;
  if (mi >= s_msg_count)
    return false;

  dbc_msg_t* m = &s_msgs[mi];

  if (m->mux_sig_index < 0)
  {
    if (!tx_pending_add(mi, 0xFFU))
      return false;
    (void)CanParams_SetBool("pending_outbox", true);
    return true;
  }

  /* Muxed: signal must belong to a mux page */
  if (s_sigs[(uint32_t)si].is_mux != 0U)
  {
    /* Multiplexor cannot schedule a page */
    return false;
  }
  if (s_sigs[(uint32_t)si].mux_val < 0)
  {
    return false;
  }

  uint8_t mux_val = (uint8_t)s_sigs[(uint32_t)si].mux_val;
  if (msg_find_page_index(mi, mux_val) < 0)
    return false;

  if (!tx_pending_add(mi, mux_val))
    return false;

  (void)CanParams_SetBool("pending_outbox", true);
  return true;
}

/* =========================
 *  Debug helpers
 * ========================= */

static bool resolve_to_msg_and_mux(const char* full_name, uint32_t* out_mi, int* out_pidx)
{
  if (full_name == NULL || out_mi == NULL || out_pidx == NULL)
    return false;

  char name[64];
  (void)strncpy(name, full_name, sizeof(name) - 1U);
  name[sizeof(name) - 1U] = '\0';
  trim_ws_inplace(name);
  if (name[0] == '\0')
    return false;

  const char* dot = strchr(name, '.');
  if (dot == NULL)
  {
    int mi = find_msg_index_by_name(name);
    if (mi < 0)
      return false;
    if (s_msgs[(uint32_t)mi].mux_sig_index >= 0)
      return false;
    *out_mi = (uint32_t)mi;
    *out_pidx = -1;
    return true;
  }

  int si = find_sig_index_by_full_name(name);
  if (si < 0)
    return false;

  uint32_t mi = (uint32_t)s_sigs[(uint32_t)si].msg_index;
  if (mi >= s_msg_count)
    return false;

  dbc_msg_t* m = &s_msgs[mi];
  if (m->mux_sig_index < 0)
  {
    *out_mi = mi;
    *out_pidx = -1;
    return true;
  }

  if (s_sigs[(uint32_t)si].is_mux != 0U)
    return false;
  if (s_sigs[(uint32_t)si].mux_val < 0)
    return false;

  int pidx = msg_find_page_index(mi, (uint8_t)s_sigs[(uint32_t)si].mux_val);
  if (pidx < 0)
    return false;

  *out_mi = mi;
  *out_pidx = pidx;
  return true;
}

bool CanSystem_DebugGetLastRxTick(const char* full_name, uint32_t* out_tick)
{
  if (out_tick == NULL)
    return false;

  uint32_t mi = 0U;
  int pidx = -1;
  if (!resolve_to_msg_and_mux(full_name, &mi, &pidx))
    return false;

  if (pidx < 0)
    *out_tick = s_msgs[mi].last_rx_tick;
  else
    *out_tick = s_msgs[mi].mux_last_rx_tick[(uint8_t)pidx];

  return true;
}

bool CanSystem_DebugGetLastTxTick(const char* full_name, uint32_t* out_tick)
{
  if (out_tick == NULL)
    return false;

  uint32_t mi = 0U;
  int pidx = -1;
  if (!resolve_to_msg_and_mux(full_name, &mi, &pidx))
    return false;

  if (pidx < 0)
    *out_tick = s_msgs[mi].last_tx_tick;
  else
    *out_tick = s_msgs[mi].mux_last_tx_tick[(uint8_t)pidx];

  return true;
}

/* =========================
 *  Deprecated legacy API (set+send)
 * ========================= */

bool CanSystem_SetBool(const char* full_name, bool value)
{
  if (full_name == NULL)
    return false;

  if (!CanParams_SetBool(full_name, value))
    return false;

  return CanSystem_Send(full_name);
}

bool CanSystem_SetInt32(const char* full_name, int32_t value)
{
  if (full_name == NULL)
    return false;

  if (!CanParams_SetInt32(full_name, value))
    return false;

  return CanSystem_Send(full_name);
}

bool CanSystem_SetFloat(const char* full_name, float value)
{
  if (full_name == NULL)
    return false;

  if (!CanParams_SetFloat(full_name, value))
    return false;

  return CanSystem_Send(full_name);
}
