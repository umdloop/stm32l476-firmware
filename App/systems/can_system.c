#include "can_system.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h> /* strtoul/strtof */

#include "can.h"
#include "main.h"

#include "can_params.h"
#include "can_config.h"

/* DBC text blob (generated from App/dbc/can_dbc_text.c) */
extern const char* g_can_dbc_text;

/* =========================
 *  DBC parsing
 * ========================= */

typedef struct
{
  uint32_t msg_id;
  uint8_t dlc;
  char name[64];
  int16_t mux_sig_index; /* index into s_sigs, -1 if none */
} dbc_msg_t;

typedef struct
{
  char full_name[64]; /* "MESSAGE.SIGNAL" */
  uint32_t msg_id;
  uint8_t start_bit;
  uint8_t length;
  bool is_signed;
  float factor;
  float offset;
  canp_type_t type;

  /* Multiplexing */
  uint8_t is_mux;    /* this signal is the multiplexor ("M") */
  int16_t mux_val;   /* -1 if not muxed, else required mux value ("m17M") */
} dbc_sig_t;

#define MAX_DBC_MSGS  (64U)
#define MAX_DBC_SIGS  (256U)

static dbc_msg_t s_msgs[MAX_DBC_MSGS];
static dbc_sig_t s_sigs[MAX_DBC_SIGS];
static uint32_t s_msg_count = 0;
static uint32_t s_sig_count = 0;

/* TX dirty queue:
 * - non-mux messages: pending_mux_count=1 with mux_val=0xFF
 * - mux messages: one entry per required mux value (0..255)
 */
#define MAX_PENDING_MUX (8U)

typedef struct
{
  uint8_t pending_mux_vals[MAX_PENDING_MUX];
  uint8_t pending_mux_count;
} tx_pending_t;

static uint8_t s_msg_dirty[MAX_DBC_MSGS];
static tx_pending_t s_msg_tx_pending[MAX_DBC_MSGS];

/* Inbox/outbox flags (backed by params "pending_inbox"/"pending_outbox") */
static uint8_t s_inbox_updated_since_tick = 0U;

/* =========================
 *  RX allowlist (software)
 * ========================= */

static bool rx_id_allowed(uint32_t std_id)
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

static int find_sig_index_by_full_name(const char* full_name)
{
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

static void tx_pending_clear(uint32_t mi)
{
  s_msg_tx_pending[mi].pending_mux_count = 0U;
}

static bool tx_pending_add(uint32_t mi, uint8_t mux_val)
{
  tx_pending_t* p = &s_msg_tx_pending[mi];

  for (uint8_t i = 0; i < p->pending_mux_count; i++)
  {
    if (p->pending_mux_vals[i] == mux_val)
    {
      return true;
    }
  }

  if (p->pending_mux_count >= MAX_PENDING_MUX)
  {
    return false;
  }

  p->pending_mux_vals[p->pending_mux_count++] = mux_val;
  return true;
}

static bool mark_dirty_for_full_name(const char* full_name)
{
  if (full_name == NULL)
  {
    return false;
  }

  int si = find_sig_index_by_full_name(full_name);
  if (si < 0)
  {
    return false;
  }

  uint32_t msg_id = s_sigs[si].msg_id;
  int mi = find_msg_index_by_id(msg_id);
  if (mi < 0)
  {
    return false;
  }

  s_msg_dirty[mi] = 1U;

  /* Queue mux value if needed */
  if (s_msgs[mi].mux_sig_index >= 0)
  {
    /* mux message */
    if (s_sigs[si].is_mux)
    {
      bool bv = false;
      int32_t iv = 0;

      if (CanParams_GetBool(full_name, &bv))
      {
        (void)tx_pending_add((uint32_t)mi, (uint8_t)(bv ? 1U : 0U));
      }
      else if (CanParams_GetInt32(full_name, &iv))
      {
        (void)tx_pending_add((uint32_t)mi, (uint8_t)(iv & 0xFF));
      }
      else
      {
        (void)tx_pending_add((uint32_t)mi, 0U);
      }
    }
    else if (s_sigs[si].mux_val >= 0)
    {
      (void)tx_pending_add((uint32_t)mi, (uint8_t)s_sigs[si].mux_val);
    }
    else
    {
      (void)tx_pending_add((uint32_t)mi, 0U);
    }
  }
  else
  {
    /* non-mux message */
    (void)tx_pending_add((uint32_t)mi, 0xFFU);
  }

  return true;
}

/* =========================
 *  Parsing utilities
 * ========================= */

static bool parse_uint(const char* s, uint32_t* out)
{
  if (s == NULL || out == NULL)
  {
    return false;
  }
  char* end = NULL;
  unsigned long v = strtoul(s, &end, 10);
  if (end == s)
  {
    return false;
  }
  *out = (uint32_t)v;
  return true;
}

static bool parse_float(const char* s, float* out)
{
  if (s == NULL || out == NULL)
  {
    return false;
  }
  char* end = NULL;
  float v = strtof(s, &end);
  if (end == s)
  {
    return false;
  }
  *out = v;
  return true;
}

/* Parse mux token from SG_ line:
 * - "M" => is mux
 * - "m17M" => muxed signal requiring mux=17
 */
static void parse_mux_token(const char* tok, uint8_t* is_mux, int16_t* mux_val)
{
  *is_mux = 0U;
  *mux_val = -1;

  if (tok == NULL || tok[0] == '\0')
  {
    return;
  }

  if (strcmp(tok, "M") == 0)
  {
    *is_mux = 1U;
    *mux_val = -1;
    return;
  }

  if (tok[0] == 'm')
  {
    /* m<number>M */
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

/* =========================
 *  DBC parsing
 * ========================= */

static void dbc_parse_reset(void)
{
  s_msg_count = 0;
  s_sig_count = 0;

  for (uint32_t i = 0; i < MAX_DBC_MSGS; i++)
  {
    s_msgs[i].msg_id = 0;
    s_msgs[i].dlc = 0;
    s_msgs[i].name[0] = '\0';
    s_msgs[i].mux_sig_index = -1;
    s_msg_dirty[i] = 0U;
    tx_pending_clear(i);
  }

  for (uint32_t i = 0; i < MAX_DBC_SIGS; i++)
  {
    s_sigs[i].full_name[0] = '\0';
    s_sigs[i].msg_id = 0;
    s_sigs[i].start_bit = 0;
    s_sigs[i].length = 0;
    s_sigs[i].is_signed = false;
    s_sigs[i].factor = 1.0f;
    s_sigs[i].offset = 0.0f;
    s_sigs[i].type = CANP_TYPE_INT32;
    s_sigs[i].is_mux = 0U;
    s_sigs[i].mux_val = -1;
  }
}

static void dbc_parse_all(void)
{
  dbc_parse_reset();
  CanParams__Reset();

  /* Always create the two global flags */
  (void)CanParams__Create("pending_inbox", CANP_TYPE_BOOL);
  (void)CanParams__Create("pending_outbox", CANP_TYPE_BOOL);
  (void)CanParams_SetBool("pending_inbox", false);
  (void)CanParams_SetBool("pending_outbox", false);

  if (g_can_dbc_text == NULL)
  {
    return;
  }

  uint32_t current_msg_id = 0;
  char current_msg_name[64] = {0};

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

    /* The generated DBC text uses tabs for indentation ("\tSG_ ...").
     * Trim leading whitespace so we correctly detect BO_/SG_ lines.
     */
    const char* lp = line;
    while (*lp == ' ' || *lp == '\t') { lp++; }

    if (strncmp(lp, "BO_ ", 4) == 0)
    {
      /* BO_ <id> <name>: <dlc> */
      char* tok = strtok((char*)lp + 4, " ");
      if (tok == NULL) continue;

      uint32_t msg_id = 0;
      if (!parse_uint(tok, &msg_id)) continue;

      tok = strtok(NULL, " :");
      if (tok == NULL) continue;

      (void)strncpy(current_msg_name, tok, sizeof(current_msg_name) - 1U);
      current_msg_name[sizeof(current_msg_name) - 1U] = '\0';
      current_msg_id = msg_id;

      tok = strtok(NULL, " ");
      if (tok == NULL) continue;

      uint32_t dlc = 8;
      (void)parse_uint(tok, &dlc);

      if (s_msg_count < MAX_DBC_MSGS)
      {
        s_msgs[s_msg_count].msg_id = current_msg_id;
        s_msgs[s_msg_count].dlc = (uint8_t)dlc;
        (void)strncpy(s_msgs[s_msg_count].name, current_msg_name, sizeof(s_msgs[s_msg_count].name) - 1U);
        s_msgs[s_msg_count].name[sizeof(s_msgs[s_msg_count].name) - 1U] = '\0';
        s_msgs[s_msg_count].mux_sig_index = -1;
        s_msg_dirty[s_msg_count] = 0U;
        tx_pending_clear(s_msg_count);
        s_msg_count++;
      }
    }
    else if (strncmp(lp, "SG_ ", 4) == 0 || strncmp(lp, "SG_ ", 4) == 0)
    {
      /* Format examples:
       *   SG_ cmd M : 0|8@1+ ...
       *   SG_ led_status m17M : 8|1@1+ ...
       *   SG_ foo : ...
       */
      const char* sg = lp + 4;

      /* Split tokens up to ':' */
      char left[128] = {0};
      const char* colon = strstr(sg, ":");
      if (colon == NULL) continue;

      size_t ln = (size_t)(colon - sg);
      if (ln >= sizeof(left)) ln = sizeof(left) - 1U;
      (void)memcpy(left, sg, ln);
      left[ln] = '\0';

      /* Tokenize left side: <sig_name> [mux_token] */
      char* name_tok = strtok(left, " \t");
      if (name_tok == NULL) continue;

      char sig_name[64] = {0};
      (void)strncpy(sig_name, name_tok, sizeof(sig_name) - 1U);
      sig_name[sizeof(sig_name) - 1U] = '\0';

      char* mux_tok = strtok(NULL, " \t"); /* may be NULL, "M", or "m17M" */
      uint8_t is_mux = 0U;
      int16_t mux_val = -1;
      if (mux_tok != NULL)
      {
        parse_mux_token(mux_tok, &is_mux, &mux_val);
      }

      const char* after_colon = colon + 1;
      while (*after_colon == ' ') after_colon++;

      uint32_t start_bit = 0;
      uint32_t sig_len = 0;

      const char* bar = strstr(after_colon, "|");
      const char* at = strstr(after_colon, "@");
      if (bar == NULL || at == NULL || bar > at) continue;

      char tmp[16] = {0};

      size_t n = (size_t)(bar - after_colon);
      if (n >= sizeof(tmp)) continue;
      (void)memcpy(tmp, after_colon, n);
      tmp[n] = '\0';
      if (!parse_uint(tmp, &start_bit)) continue;

      n = (size_t)(at - (bar + 1));
      if (n >= sizeof(tmp)) continue;
      (void)memcpy(tmp, bar + 1, n);
      tmp[n] = '\0';
      if (!parse_uint(tmp, &sig_len)) continue;

      bool is_signed = false;
      if (at[2] == '-') is_signed = true;

      float factor = 1.0f;
      float offset = 0.0f;

      const char* lp2 = strstr(after_colon, "(");
      const char* rp2 = strstr(after_colon, ")");
      if (lp2 && rp2 && rp2 > lp2)
      {
        char pair[64] = {0};
        size_t pn = (size_t)(rp2 - (lp2 + 1));
        if (pn < sizeof(pair))
        {
          (void)memcpy(pair, lp2 + 1, pn);
          pair[pn] = '\0';

          char* comma = strchr(pair, ',');
          if (comma)
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
      else if (sig_len == 1U)
        type = CANP_TYPE_BOOL;
      else
        type = CANP_TYPE_INT32;

      if (s_sig_count < MAX_DBC_SIGS)
      {
        (void)build_full_name(s_sigs[s_sig_count].full_name,
                              sizeof(s_sigs[s_sig_count].full_name),
                              current_msg_name,
                              sig_name);

        s_sigs[s_sig_count].msg_id = current_msg_id;
        s_sigs[s_sig_count].start_bit = (uint8_t)start_bit;
        s_sigs[s_sig_count].length = (uint8_t)sig_len;
        s_sigs[s_sig_count].is_signed = is_signed;
        s_sigs[s_sig_count].factor = factor;
        s_sigs[s_sig_count].offset = offset;
        s_sigs[s_sig_count].type = type;
        s_sigs[s_sig_count].is_mux = is_mux;
        s_sigs[s_sig_count].mux_val = mux_val;

        (void)CanParams__Create(s_sigs[s_sig_count].full_name, type);

        if (is_mux != 0U)
        {
          int mi = find_msg_index_by_id(current_msg_id);
          if (mi >= 0)
          {
            s_msgs[mi].mux_sig_index = (int16_t)s_sig_count;
          }
        }

        s_sig_count++;
      }
    }
  }
}

/* =========================
 *  Bit helpers
 * ========================= */

static uint32_t extract_bits_le(const uint8_t* data, uint8_t start_bit, uint8_t length)
{
  uint32_t out = 0;
  for (uint8_t i = 0; i < length; i++)
  {
    uint16_t bit_index = (uint16_t)start_bit + i;
    uint16_t byte_index = bit_index / 8U;
    uint8_t bit_in_byte = (uint8_t)(bit_index % 8U);
    uint8_t bit = (data[byte_index] >> bit_in_byte) & 0x1U;
    out |= ((uint32_t)bit << i);
  }
  return out;
}

static int32_t sign_extend(uint32_t raw, uint8_t bitlen)
{
  if (bitlen == 0U || bitlen >= 32U)
    return (int32_t)raw;

  uint32_t sign_bit = 1UL << (bitlen - 1U);
  if (raw & sign_bit)
  {
    uint32_t mask = ~((1UL << bitlen) - 1U);
    raw |= mask;
  }
  return (int32_t)raw;
}

/* =========================
 *  RX decode + FIFO drain
 * ========================= */

static void handle_rx_frame(uint32_t std_id, const uint8_t* data, uint8_t dlc)
{
  (void)dlc;

  if (!rx_id_allowed(std_id))
  {
    return;
  }

  int mi = find_msg_index_by_id(std_id);
  int16_t mux_sig = -1;
  uint32_t mux_val = 0;

  if (mi >= 0)
  {
    mux_sig = s_msgs[mi].mux_sig_index;
  }

  if (mux_sig >= 0)
  {
    const dbc_sig_t* ms = &s_sigs[(uint32_t)mux_sig];
    mux_val = extract_bits_le(data, ms->start_bit, ms->length);

    if (ms->type == CANP_TYPE_BOOL)
    {
      if (CanParams__UpdateBool(ms->full_name, (uint8_t)(mux_val != 0U)))
        s_inbox_updated_since_tick = 1U;
    }
    else
    {
      if (CanParams__UpdateInt32(ms->full_name, (int32_t)mux_val))
        s_inbox_updated_since_tick = 1U;
    }
  }

  for (uint32_t i = 0; i < s_sig_count; i++)
  {
    const dbc_sig_t* s = &s_sigs[i];
    if (s->msg_id != std_id)
      continue;

    if (mux_sig >= 0)
    {
      if (s->is_mux != 0U)
      {
        continue;
      }
      if (s->mux_val >= 0)
      {
        if ((uint32_t)s->mux_val != mux_val)
        {
          continue;
        }
      }
    }

    uint32_t raw = extract_bits_le(data, s->start_bit, s->length);
    bool updated_any = false;

    if (s->type == CANP_TYPE_BOOL)
    {
      if (CanParams__UpdateBool(s->full_name, (uint8_t)(raw != 0U)))
        updated_any = true;
    }
    else if (s->type == CANP_TYPE_FLOAT)
    {
      int32_t signed_raw = s->is_signed ? sign_extend(raw, s->length) : (int32_t)raw;
      float phys = ((float)signed_raw) * s->factor + s->offset;
      if (CanParams__UpdateFloat(s->full_name, phys))
        updated_any = true;
    }
    else
    {
      int32_t signed_raw = s->is_signed ? sign_extend(raw, s->length) : (int32_t)raw;
      if (CanParams__UpdateInt32(s->full_name, signed_raw))
        updated_any = true;
    }

    if (updated_any)
    {
      s_inbox_updated_since_tick = 1U;
    }
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

static void insert_bits_le(uint8_t* data, uint8_t start_bit, uint8_t length, uint32_t value)
{
  for (uint8_t i = 0; i < length; i++)
  {
    uint16_t bit_index = (uint16_t)start_bit + i;
    uint16_t byte_index = bit_index / 8U;
    uint8_t bit_in_byte = (uint8_t)(bit_index % 8U);

    uint8_t bit = (uint8_t)((value >> i) & 0x1U);

    data[byte_index] &= (uint8_t)~(1U << bit_in_byte);
    data[byte_index] |= (uint8_t)(bit << bit_in_byte);
  }
}

static bool encode_raw_from_param(const dbc_sig_t* s, uint32_t* out_raw)
{
  if (s == NULL || out_raw == NULL)
    return false;

  if (s->type == CANP_TYPE_BOOL)
  {
    bool bv = false;
    if (!CanParams_GetBool(s->full_name, &bv))
      return false;
    *out_raw = bv ? 1U : 0U;
    return true;
  }
  else if (s->type == CANP_TYPE_FLOAT)
  {
    float fv = 0.0f;
    if (!CanParams_GetFloat(s->full_name, &fv))
      return false;

    if (s->factor == 0.0f)
      return false;

    float fr = (fv - s->offset) / s->factor;
    int32_t ir = (int32_t)fr;
    *out_raw = (uint32_t)ir;
    return true;
  }
  else
  {
    int32_t iv = 0;
    if (!CanParams_GetInt32(s->full_name, &iv))
      return false;
    *out_raw = (uint32_t)iv;
    return true;
  }
}

static void transmit_one(uint32_t mi, uint8_t mux_to_send)
{
  uint8_t data[8] = {0};

  int16_t mux_sig = s_msgs[mi].mux_sig_index;
  if (mux_sig >= 0)
  {
    const dbc_sig_t* ms = &s_sigs[(uint32_t)mux_sig];
    insert_bits_le(data, ms->start_bit, ms->length, (uint32_t)mux_to_send);
  }

  for (uint32_t si = 0; si < s_sig_count; si++)
  {
    const dbc_sig_t* s = &s_sigs[si];
    if (s->msg_id != s_msgs[mi].msg_id)
      continue;

    if (mux_sig >= 0)
    {
      if (s->is_mux != 0U)
      {
        continue;
      }

      if (s->mux_val >= 0)
      {
        if ((uint8_t)s->mux_val != mux_to_send)
        {
          continue;
        }
      }
    }

    uint32_t raw = 0U;
    if (!encode_raw_from_param(s, &raw))
    {
      raw = 0U;
    }

    insert_bits_le(data, s->start_bit, s->length, raw);
  }

  CAN_TxHeaderTypeDef txh;
  txh.StdId = s_msgs[mi].msg_id;
  txh.ExtId = 0;
  txh.IDE = CAN_ID_STD;
  txh.RTR = CAN_RTR_DATA;
  txh.DLC = s_msgs[mi].dlc;
  txh.TransmitGlobalTime = DISABLE;

  uint32_t mbx = 0;
  (void)HAL_CAN_AddTxMessage(&hcan1, &txh, data, &mbx);
}

static void transmit_dirty_messages(void)
{
  for (uint32_t mi = 0; mi < s_msg_count; mi++)
  {
    if (s_msg_dirty[mi] == 0U)
      continue;

    tx_pending_t* pend = &s_msg_tx_pending[mi];
    if (pend->pending_mux_count == 0U)
    {
      if (s_msgs[mi].mux_sig_index >= 0)
      {
        transmit_one(mi, 0U);
      }
      else
      {
        transmit_one(mi, 0xFFU);
      }
    }
    else
    {
      if (s_msgs[mi].mux_sig_index >= 0)
      {
        for (uint8_t k = 0; k < pend->pending_mux_count; k++)
        {
          transmit_one(mi, pend->pending_mux_vals[k]);
        }
      }
      else
      {
        transmit_one(mi, 0xFFU);
      }
    }

    s_msg_dirty[mi] = 0U;
    tx_pending_clear(mi);
  }

  (void)CanParams_SetBool("pending_outbox", false);
}

/* =========================
 *  Round-robin controller
 * ========================= */

void can_system_controller(void)
{
  static uint8_t s_inited = 0U;

  if (!s_inited)
  {
    dbc_parse_all();

    can_apply_filters();

    (void)HAL_CAN_Start(&hcan1);
    (void)HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);

    s_inited = 1U;
  }

  process_rx_fifo0();

  if (s_inbox_updated_since_tick != 0U)
    (void)CanParams_SetBool("pending_inbox", true);
  else
    (void)CanParams_SetBool("pending_inbox", false);

  s_inbox_updated_since_tick = 0U;

  bool pending = false;
  if (CanParams_GetBool("pending_outbox", &pending) && pending)
  {
    transmit_dirty_messages();
  }
}

/* =========================
 *  External TX request API
 * ========================= */

bool CanSystem_SetBool(const char* full_name, bool value)
{
    if (full_name == NULL)
        return false;

    if (strcmp(full_name, "pending_inbox") == 0 ||
        strcmp(full_name, "pending_outbox") == 0)
        return false;

    if (!mark_dirty_for_full_name(full_name))
        return false;

    /* Force-set: ignore "no change" result */
    (void)CanParams_SetBool(full_name, value);

    (void)CanParams_SetBool("pending_outbox", true);
    return true;
}

bool CanSystem_SetInt32(const char* full_name, int32_t value)
{
    if (full_name == NULL) return false;
    if (strcmp(full_name, "pending_inbox") == 0 || strcmp(full_name, "pending_outbox") == 0) return false;

    if (!mark_dirty_for_full_name(full_name))
        return false;

    // Set param, but don't treat "no change" as failure
    (void)CanParams_SetInt32(full_name, value);

    (void)CanParams_SetBool("pending_outbox", true);
    return true;
}


bool CanSystem_SetFloat(const char* full_name, float value)
{
    if (full_name == NULL)
        return false;

    if (strcmp(full_name, "pending_inbox") == 0 ||
        strcmp(full_name, "pending_outbox") == 0)
        return false;

    if (!mark_dirty_for_full_name(full_name))
        return false;

    /* Force-set: ignore "no change" result */
    (void)CanParams_SetFloat(full_name, value);

    (void)CanParams_SetBool("pending_outbox", true);
    return true;
}

