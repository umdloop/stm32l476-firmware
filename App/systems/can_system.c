#include "can_system.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h> /* strtoul/strtol/strtof */

#include "can.h"
#include "main.h"

/* DBC text blob (generated from App/dbc/file.dbc) */
extern const char* g_can_dbc_text;

/* Parameter internals owned by can_params.c */
typedef enum { CANP_TYPE_BOOL = 0, CANP_TYPE_INT32, CANP_TYPE_FLOAT } canp_type_t;
extern void CanParams__Reset(void);
extern bool CanParams__Create(const char* full_name, canp_type_t type);
extern bool CanParams__UpdateBool(const char* full_name, uint8_t value);
extern bool CanParams__UpdateInt32(const char* full_name, int32_t value);
extern bool CanParams__UpdateFloat(const char* full_name, float value);

/* =========================
 *  DBC structures
 * ========================= */

typedef struct
{
  uint32_t can_id;     /* standard ID only for now */
  uint8_t  dlc;
  char     name[32];
} dbc_msg_t;

typedef struct
{
  uint32_t msg_id;
  char     msg_name[32];
  char     sig_name[32];
  char     full_name[64]; /* "MSG.SIG" */

  uint16_t start_bit;
  uint16_t bit_len;

  uint8_t  is_signed;     /* + / - */
  uint8_t  is_intel;      /* @1 only supported */
  float    factor;
  float    offset;

  /* Multiplexing */
  uint8_t  is_mux;        /* signal is multiplexor */
  int16_t  mux_val;       /* -1 if not muxed, else required mux value */

  /* Chosen storage type */
  canp_type_t ptype;
} dbc_sig_t;

#define DBC_MAX_MSGS    (64U)
#define DBC_MAX_SIGS    (256U)

static dbc_msg_t s_msgs[DBC_MAX_MSGS];
static size_t s_msg_count = 0;

static dbc_sig_t s_sigs[DBC_MAX_SIGS];
static size_t s_sig_count = 0;

typedef struct
{
  uint32_t msg_id;
  int16_t mux_sig_index; /* index into s_sigs, -1 if none */
} dbc_mux_map_t;

static dbc_mux_map_t s_mux_map[DBC_MAX_MSGS];
static size_t s_mux_map_count = 0;

static uint8_t s_inited = 0;

/* =========================
 *  Helpers: parsing
 * ========================= */

static const char* skip_ws(const char* p)
{
  while (*p == ' ' || *p == '\t' || *p == '\r')
    p++;
  return p;
}

static bool starts_with(const char* p, const char* pref)
{
  return (strncmp(p, pref, strlen(pref)) == 0);
}

static void copy_token(char* out, size_t out_sz, const char* start, const char* end)
{
  size_t n = (size_t)(end - start);
  if (n >= out_sz) n = out_sz - 1;
  memcpy(out, start, n);
  out[n] = '\0';
}

static const char* read_line(const char* p, const char** line_start, const char** line_end)
{
  if (*p == '\0')
  {
    *line_start = NULL;
    *line_end = NULL;
    return p;
  }

  *line_start = p;
  while (*p != '\n' && *p != '\0')
    p++;
  *line_end = p;

  if (*p == '\n')
    p++;

  return p;
}

static int find_mux_sig_index(uint32_t msg_id)
{
  for (size_t i = 0; i < s_mux_map_count; i++)
  {
    if (s_mux_map[i].msg_id == msg_id)
      return s_mux_map[i].mux_sig_index;
  }
  return -1;
}

static void set_mux_sig_index(uint32_t msg_id, int16_t sig_index)
{
  for (size_t i = 0; i < s_mux_map_count; i++)
  {
    if (s_mux_map[i].msg_id == msg_id)
    {
      s_mux_map[i].mux_sig_index = sig_index;
      return;
    }
  }

  if (s_mux_map_count < DBC_MAX_MSGS)
  {
    s_mux_map[s_mux_map_count].msg_id = msg_id;
    s_mux_map[s_mux_map_count].mux_sig_index = sig_index;
    s_mux_map_count++;
  }
}

static void parse_bo_line(const char* ls, const char* le)
{
  if (s_msg_count >= DBC_MAX_MSGS)
    return;

  const char* p = ls;
  p = skip_ws(p);
  if (!starts_with(p, "BO_"))
    return;
  p += 3;
  p = skip_ws(p);

  char idbuf[16] = {0};
  const char* id_start = p;
  while (p < le && *p >= '0' && *p <= '9')
    p++;
  copy_token(idbuf, sizeof(idbuf), id_start, p);
  uint32_t msg_id = (uint32_t)strtoul(idbuf, NULL, 10);

  p = skip_ws(p);

  const char* name_start = p;
  while (p < le && *p != ':' && *p != ' ')
    p++;
  char namebuf[32] = {0};
  copy_token(namebuf, sizeof(namebuf), name_start, p);

  while (p < le && *p != ':')
    p++;
  if (p >= le || *p != ':')
    return;
  p++;
  p = skip_ws(p);

  char dlcbuf[8] = {0};
  const char* dlc_start = p;
  while (p < le && *p >= '0' && *p <= '9')
    p++;
  copy_token(dlcbuf, sizeof(dlcbuf), dlc_start, p);
  uint8_t dlc = (uint8_t)strtoul(dlcbuf, NULL, 10);

  dbc_msg_t* m = &s_msgs[s_msg_count++];
  memset(m, 0, sizeof(*m));
  m->can_id = msg_id;
  m->dlc = dlc;
  strncpy(m->name, namebuf, sizeof(m->name) - 1);

  set_mux_sig_index(msg_id, -1);
}

static void parse_sg_line(const char* current_msg_name, uint32_t current_msg_id,
                          const char* ls, const char* le)
{
  if (s_sig_count >= DBC_MAX_SIGS)
    return;

  const char* p = ls;
  p = skip_ws(p);
  if (!starts_with(p, "SG_"))
    return;
  p += 3;
  p = skip_ws(p);

  const char* sig_start = p;
  while (p < le && *p != ' ' && *p != '\t')
    p++;
  char sig_name[32] = {0};
  copy_token(sig_name, sizeof(sig_name), sig_start, p);
  p = skip_ws(p);

  uint8_t is_mux = 0;
  int16_t mux_val = -1;

  if (p < le && *p != ':')
  {
    const char* tok_start = p;
    while (p < le && *p != ' ' && *p != '\t' && *p != ':')
      p++;
    char mtoken[16] = {0};
    copy_token(mtoken, sizeof(mtoken), tok_start, p);
    p = skip_ws(p);

    if (strcmp(mtoken, "M") == 0)
    {
      is_mux = 1;
      mux_val = -1;
    }
    else if (mtoken[0] == 'm')
    {
      char* endptr = NULL;
      long v = strtol(&mtoken[1], &endptr, 10);
      if (endptr && *endptr == 'M')
      {
        mux_val = (int16_t)v;
      }
    }
  }

  while (p < le && *p != ':')
    p++;
  if (p >= le || *p != ':')
    return;
  p++;
  p = skip_ws(p);

  char sbuf[16] = {0};
  const char* sb_start = p;
  while (p < le && *p >= '0' && *p <= '9')
    p++;
  copy_token(sbuf, sizeof(sbuf), sb_start, p);
  uint16_t start_bit = (uint16_t)strtoul(sbuf, NULL, 10);

  if (p >= le || *p != '|')
    return;
  p++;

  char lbuf[16] = {0};
  const char* l_start = p;
  while (p < le && *p >= '0' && *p <= '9')
    p++;
  copy_token(lbuf, sizeof(lbuf), l_start, p);
  uint16_t bit_len = (uint16_t)strtoul(lbuf, NULL, 10);

  if (p >= le || *p != '@')
    return;
  p++;

  uint8_t is_intel = 0;
  if (p < le && *p == '1')
    is_intel = 1;
  p++;

  uint8_t is_signed = 0;
  if (p < le && *p == '+')
    is_signed = 0;
  else if (p < le && *p == '-')
    is_signed = 1;

  while (p < le && *p != '(')
    p++;
  float factor = 1.0f;
  float offset = 0.0f;

  if (p < le && *p == '(')
  {
    p++;
    char fbuf[24] = {0};
    const char* f_start = p;
    while (p < le && *p != ',' && *p != ')')
      p++;
    copy_token(fbuf, sizeof(fbuf), f_start, p);
    factor = strtof(fbuf, NULL);

    if (p < le && *p == ',')
      p++;

    char obuf[24] = {0};
    const char* o_start = p;
    while (p < le && *p != ')')
      p++;
    copy_token(obuf, sizeof(obuf), o_start, p);
    offset = strtof(obuf, NULL);
  }

  dbc_sig_t* s = &s_sigs[s_sig_count++];
  memset(s, 0, sizeof(*s));
  s->msg_id = current_msg_id;
  strncpy(s->msg_name, current_msg_name, sizeof(s->msg_name) - 1);
  strncpy(s->sig_name, sig_name, sizeof(s->sig_name) - 1);

  strncpy(s->full_name, current_msg_name, sizeof(s->full_name) - 1);
  strncat(s->full_name, ".", sizeof(s->full_name) - strlen(s->full_name) - 1);
  strncat(s->full_name, sig_name, sizeof(s->full_name) - strlen(s->full_name) - 1);

  s->start_bit = start_bit;
  s->bit_len = bit_len;
  s->is_intel = is_intel;
  s->is_signed = is_signed;
  s->factor = factor;
  s->offset = offset;
  s->is_mux = is_mux;
  s->mux_val = mux_val;

  if (bit_len == 1 && is_signed == 0)
    s->ptype = CANP_TYPE_BOOL;
  else if (factor == 1.0f && offset == 0.0f)
    s->ptype = CANP_TYPE_INT32;
  else
    s->ptype = CANP_TYPE_FLOAT;

  (void)CanParams__Create(s->full_name, s->ptype);

  if (is_mux)
  {
    set_mux_sig_index(current_msg_id, (int16_t)(s_sig_count - 1));
  }
}

static void dbc_parse_all(void)
{
  s_msg_count = 0;
  s_sig_count = 0;
  s_mux_map_count = 0;

  CanParams__Reset();

  const char* p = g_can_dbc_text;
  const char* ls = NULL;
  const char* le = NULL;

  uint32_t current_msg_id = 0;
  char current_msg_name[32] = {0};
  uint8_t have_msg = 0;

  while (*p != '\0')
  {
    p = read_line(p, &ls, &le);
    if (ls == NULL || le == NULL)
      break;

    const char* line = skip_ws(ls);
    if (line >= le)
      continue;

    if (starts_with(line, "BO_"))
    {
      parse_bo_line(line, le);

      if (s_msg_count > 0)
      {
        current_msg_id = s_msgs[s_msg_count - 1].can_id;
        strncpy(current_msg_name, s_msgs[s_msg_count - 1].name, sizeof(current_msg_name) - 1);
        have_msg = 1;
      }
      continue;
    }

    if (starts_with(line, "SG_") && have_msg)
    {
      parse_sg_line(current_msg_name, current_msg_id, line, le);
      continue;
    }
  }
}

/* =========================
 *  Helpers: decode (Intel only)
 * ========================= */

static uint64_t extract_bits_intel(const uint8_t* data, uint16_t start_bit, uint16_t bit_len)
{
  uint64_t w = 0;
  for (int i = 0; i < 8; i++)
  {
    w |= ((uint64_t)data[i]) << (8U * (uint32_t)i);
  }

  uint64_t mask = (bit_len == 64U) ? ~0ULL : ((1ULL << bit_len) - 1ULL);
  return (w >> start_bit) & mask;
}

static int32_t sign_extend(uint32_t raw, uint16_t bit_len)
{
  if (bit_len == 0 || bit_len >= 32U)
    return (int32_t)raw;

  uint32_t sign_bit = 1U << (bit_len - 1U);
  if (raw & sign_bit)
  {
    uint32_t ext_mask = ~((1U << bit_len) - 1U);
    raw |= ext_mask;
  }
  return (int32_t)raw;
}

static void decode_and_update_params(uint32_t std_id, const uint8_t* data)
{
  int16_t mux_sig_index = (int16_t)find_mux_sig_index(std_id);
  int32_t mux_value = 0;
  uint8_t have_mux_value = 0;

  if (mux_sig_index >= 0 && (size_t)mux_sig_index < s_sig_count)
  {
    const dbc_sig_t* muxs = &s_sigs[mux_sig_index];
    if (muxs->is_intel)
    {
      uint64_t raw = extract_bits_intel(data, muxs->start_bit, muxs->bit_len);
      mux_value = (int32_t)raw;
      have_mux_value = 1;
    }
  }

  for (size_t i = 0; i < s_sig_count; i++)
  {
    const dbc_sig_t* s = &s_sigs[i];
    if (s->msg_id != std_id)
      continue;

    if (!s->is_intel)
      continue;

    if (s->mux_val >= 0)
    {
      if (!have_mux_value || mux_value != s->mux_val)
        continue;
    }

    uint32_t raw32 = (uint32_t)extract_bits_intel(data, s->start_bit, s->bit_len);

    if (s->ptype == CANP_TYPE_BOOL)
    {
      (void)CanParams__UpdateBool(s->full_name, (raw32 & 0x1U) ? 1U : 0U);
      continue;
    }

    if (s->ptype == CANP_TYPE_INT32)
    {
      int32_t val = s->is_signed ? sign_extend(raw32, s->bit_len) : (int32_t)raw32;
      (void)CanParams__UpdateInt32(s->full_name, val);
      continue;
    }

    /* float */
    {
      float phys = 0.0f;
      if (s->is_signed)
      {
        int32_t sval = sign_extend(raw32, s->bit_len);
        phys = ((float)sval) * s->factor + s->offset;
      }
      else
      {
        phys = ((float)raw32) * s->factor + s->offset;
      }
      (void)CanParams__UpdateFloat(s->full_name, phys);
    }
  }
}

/* =========================
 *  RX processing (used by IRQ and by polling fallback)
 * ========================= */

static void process_rx_fifo0(void)
{
  CAN_RxHeaderTypeDef rxHeader = {0};
  uint8_t rxData[8] = {0};

  while (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) > 0)
  {
    if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK)
    {
      if (rxHeader.IDE == CAN_ID_STD)
      {
        decode_and_update_params(rxHeader.StdId, rxData);
      }
    }
  }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  if (hcan != &hcan1)
    return;

  process_rx_fifo0();
}

/* =========================
 *  Init + controller
 * ========================= */

static void can_system_init_once(void)
{
  dbc_parse_all();

  /* Filters: accept all for now */
  CAN_FilterTypeDef filter = {0};
  filter.FilterBank = 0;
  filter.FilterMode = CAN_FILTERMODE_IDMASK;
  filter.FilterScale = CAN_FILTERSCALE_32BIT;
  filter.FilterIdHigh = 0x0000;
  filter.FilterIdLow  = 0x0000;
  filter.FilterMaskIdHigh = 0x0000;
  filter.FilterMaskIdLow  = 0x0000;
  filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  filter.FilterActivation = ENABLE;
  filter.SlaveStartFilterBank = 14;

  if (HAL_CAN_ConfigFilter(&hcan1, &filter) != HAL_OK)
    Error_Handler();

  if (HAL_CAN_Start(&hcan1) != HAL_OK)
    Error_Handler();

  /* Enable notifications (interrupt-driven RX) */
  (void)HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
}

void can_system_controller(void)
{
  if (!s_inited)
  {
    s_inited = 1U;
    can_system_init_once();
  }

  /* Fallback polling:
   * If NVIC/IRQ handler isn't wired, this still drains RX and updates params. */
  process_rx_fifo0();

  /* TX path later; currently no send signals configured */
}
