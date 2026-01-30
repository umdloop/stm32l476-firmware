#include "servo_system.h"

#include "main.h"
#include "can_params.h"
#include "can_system.h"

#include <string.h>
#include <stdbool.h>

/* =========================
 *  Servo model definitions
 * ========================= */

typedef struct
{
  bool position;
  bool velocity;
} ServoModes_t;

typedef struct
{
  bool has_feedback;
} ServoFeedback_t;

typedef struct
{
  const char* name;

  ServoModes_t modes;

  uint16_t pwm_min_us;
  uint16_t pwm_max_us;

  float max_rotation_deg;
  float max_diff_position_deg;

  float travel_deg_per_us;

  uint16_t vel_neutral_us;
  float    vel_deg_s_per_us;

  float max_speed_deg_s;

  ServoFeedback_t feedback;

} ServoDef_t;

static const ServoDef_t s_servo_defs[] =
{
  {
    .name = "NONE",
    .modes = { .position = false, .velocity = false },
    .pwm_min_us = 0, .pwm_max_us = 0,
    .max_rotation_deg = 0.0f, .max_diff_position_deg = 0.0f,
    .travel_deg_per_us = 0.0f,
    .vel_neutral_us = 1500, .vel_deg_s_per_us = 0.0f,
    .max_speed_deg_s = 0.0f,
    .feedback = { .has_feedback = false },
  },
  {
    .name = "Hitec HS-645MG",
    .modes = { .position = true, .velocity = false },

    .pwm_min_us = 553,
    .pwm_max_us = 2520,

    .max_rotation_deg = 197.0f,
    .max_diff_position_deg = 197.0f,

    .travel_deg_per_us = 0.100f,

    .vel_neutral_us = 1500,
    .vel_deg_s_per_us = 0.0f,

    .max_speed_deg_s = 250.0f,

    .feedback = { .has_feedback = false },
  }
};

static const uint8_t s_servo_defs_count = (uint8_t)(sizeof(s_servo_defs) / sizeof(s_servo_defs[0]));

/* =========================
 *  Hardware mapping
 * ========================= */

typedef struct
{
  GPIO_TypeDef* pwm_port;
  uint16_t      pwm_pin;
  uint32_t      pwm_af;

  TIM_TypeDef*  tim;
  uint8_t       channel;

  GPIO_TypeDef* vcc_port;
  uint16_t      vcc_pin;

  GPIO_TypeDef* fbk_port;
  uint16_t      fbk_pin;
  bool          has_fbk_pin;

} ServoPortHw_t;

static ServoPortHw_t s_hw[SERVO_PORT_COUNT] =
{
  //{ GPIOB, GPIO_PIN_7,  GPIO_AF2_TIM4, TIM4, 2, GPIOC, GPIO_PIN_15, GPIOC, GPIO_PIN_0, true },
  { GPIOA, GPIO_PIN_15, GPIO_AF1_TIM2, TIM2, 1, GPIOC, GPIO_PIN_11, NULL, 0, false },
  { GPIOB, GPIO_PIN_8,  GPIO_AF2_TIM4, TIM4, 3, GPIOC, GPIO_PIN_14, GPIOC, GPIO_PIN_1, true },
  { GPIOA, GPIO_PIN_3,  GPIO_AF1_TIM2, TIM2, 4, GPIOC, GPIO_PIN_7,  GPIOC, GPIO_PIN_2, true },
  { GPIOA, GPIO_PIN_7,  GPIO_AF2_TIM3, TIM3, 2, GPIOA, GPIO_PIN_8,  GPIOC, GPIO_PIN_3, true },
  { GPIOB, GPIO_PIN_0,  GPIO_AF2_TIM3, TIM3, 3, GPIOB, GPIO_PIN_14, NULL, 0, false },
  { GPIOC, GPIO_PIN_6,  GPIO_AF2_TIM3, TIM3, 1, GPIOB, GPIO_PIN_10, NULL, 0, false },
  //{ GPIOA, GPIO_PIN_15, GPIO_AF1_TIM2, TIM2, 1, GPIOC, GPIO_PIN_11, NULL, 0, false },
  { GPIOB, GPIO_PIN_7,  GPIO_AF2_TIM4, TIM4, 2, GPIOC, GPIO_PIN_15, GPIOC, GPIO_PIN_0, true },
  { GPIOB, GPIO_PIN_3,  GPIO_AF1_TIM2, TIM2, 2, GPIOC, GPIO_PIN_12, NULL, 0, false },
};

/* =========================
 *  State
 * ========================= */

typedef struct
{
  uint8_t  model_id;
  float    target_position_deg;
  float    target_velocity_deg_s;
  uint16_t current_pwm_us;
} ServoPortState_t;

static ServoPortState_t s_ports[SERVO_PORT_COUNT];
static uint8_t s_inited = 0U;

/* =========================
 *  CAN integration
 * ========================= */

#define SERVO_CAN_COUNT (6u)

static const char* s_can_general[SERVO_CAN_COUNT] =
{
  "SERVO_PCB_C.general_0",
  "SERVO_PCB_C.general_1",
  "SERVO_PCB_C.general_2",
  "SERVO_PCB_C.general_3",
  "SERVO_PCB_C.general_4",
  "SERVO_PCB_C.general_5",
};

static const char* s_can_pos_tgt[SERVO_CAN_COUNT] =
{
  "SERVO_PCB_C.motor_position_target_0",
  "SERVO_PCB_C.motor_position_target_1",
  "SERVO_PCB_C.motor_position_target_2",
  "SERVO_PCB_C.motor_position_target_3",
  "SERVO_PCB_C.motor_position_target_4",
  "SERVO_PCB_C.motor_position_target_5",
};

static const char* s_can_vel_tgt[SERVO_CAN_COUNT] =
{
  "SERVO_PCB_C.motor_velocity_target_0",
  "SERVO_PCB_C.motor_velocity_target_1",
  "SERVO_PCB_C.motor_velocity_target_2",
  "SERVO_PCB_C.motor_velocity_target_3",
  "SERVO_PCB_C.motor_velocity_target_4",
  "SERVO_PCB_C.motor_velocity_target_5",
};

static const char* s_can_pos_out[SERVO_CAN_COUNT] =
{
  "SERVO_PCB_R.motor_position_0",
  "SERVO_PCB_R.motor_position_1",
  "SERVO_PCB_R.motor_position_2",
  "SERVO_PCB_R.motor_position_3",
  "SERVO_PCB_R.motor_position_4",
  "SERVO_PCB_R.motor_position_5",
};

static const char* s_can_vel_out[SERVO_CAN_COUNT] =
{
  "SERVO_PCB_R.motor_velocity_0",
  "SERVO_PCB_R.motor_velocity_1",
  "SERVO_PCB_R.motor_velocity_2",
  "SERVO_PCB_R.motor_velocity_3",
  "SERVO_PCB_R.motor_velocity_4",
  "SERVO_PCB_R.motor_velocity_5",
};

static uint8_t s_rx_inited = 0U;
static int32_t s_last_general[SERVO_CAN_COUNT];
static int32_t s_last_pos_tgt[SERVO_CAN_COUNT];
static int32_t s_last_vel_tgt[SERVO_CAN_COUNT];

/* Weak callbacks */
__attribute__((weak)) void ServoSystem_OnSetZero(uint8_t port)        { (void)port; }
__attribute__((weak)) void ServoSystem_OnRequestVectors(uint8_t port) { (void)port; }
__attribute__((weak)) void ServoSystem_OnStopMotor(uint8_t port)      { (void)port; }
__attribute__((weak)) void ServoSystem_OnShutdownMotor(uint8_t port)  { (void)port; }
__attribute__((weak)) void ServoSystem_OnClearErrors(uint8_t port)    { (void)port; }

/* =========================
 *  Helpers
 * ========================= */

static bool is_port_valid(uint8_t port) { return (port < SERVO_PORT_COUNT); }

static const ServoDef_t* get_def(uint8_t model_id)
{
  if (model_id >= s_servo_defs_count) return NULL;
  return &s_servo_defs[model_id];
}

static float clampf(float x, float lo, float hi)
{
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

/* ACTIVE-HIGH enable */
static void set_vcc(uint8_t port, bool on)
{
  HAL_GPIO_WritePin(s_hw[port].vcc_port, s_hw[port].vcc_pin,
                    on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* TIM CCR write */
static void tim_set_ccr(TIM_TypeDef* tim, uint8_t ch, uint16_t value)
{
  switch (ch)
  {
    case 1: tim->CCR1 = value; break;
    case 2: tim->CCR2 = value; break;
    case 3: tim->CCR3 = value; break;
    case 4: tim->CCR4 = value; break;
    default: break;
  }
}

static void set_pwm_us(uint8_t port, uint16_t pwm_us)
{
  tim_set_ccr(s_hw[port].tim, s_hw[port].channel, pwm_us);
  s_ports[port].current_pwm_us = pwm_us;
}

/* Position -> PWM */
static bool compute_pwm_us_for_position(const ServoDef_t* def, float position_deg, uint16_t* out_pwm_us)
{
  if ((def == NULL) || (out_pwm_us == NULL)) return false;
  if (!def->modes.position) return false;

  position_deg = clampf(position_deg, 0.0f, def->max_rotation_deg);

  float us = (position_deg / def->travel_deg_per_us) + (float)def->pwm_min_us;
  us = clampf(us, (float)def->pwm_min_us, (float)def->pwm_max_us);

  *out_pwm_us = (uint16_t)(us + 0.5f);
  return true;
}

/* PWM -> position estimate */
static bool compute_position_from_pwm(const ServoDef_t* def, uint16_t pwm_us, int32_t* out_pos_deg)
{
  if ((def == NULL) || (out_pos_deg == NULL)) return false;
  if (!def->modes.position) return false;

  float pos = ((float)pwm_us - (float)def->pwm_min_us) * def->travel_deg_per_us;
  pos = clampf(pos, 0.0f, def->max_rotation_deg);

  *out_pos_deg = (int32_t)(pos + 0.5f);
  return true;
}

/* =========================
 *  TIM init (register-level)
 * ========================= */

static void tim_enable_clock(TIM_TypeDef* tim)
{
  if (tim == TIM2) __HAL_RCC_TIM2_CLK_ENABLE();
  else if (tim == TIM3) __HAL_RCC_TIM3_CLK_ENABLE();
  else if (tim == TIM4) __HAL_RCC_TIM4_CLK_ENABLE();
}

static uint32_t tim_get_clock_hz_apb1(void)
{
  uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
  uint32_t ppre1 = (RCC->CFGR & RCC_CFGR_PPRE1);
  bool apb1_div1 = (ppre1 == RCC_CFGR_PPRE1_DIV1);
  return apb1_div1 ? pclk1 : (pclk1 * 2u);
}

static void tim_init_1mhz_50hz(TIM_TypeDef* tim)
{
  tim_enable_clock(tim);

  uint32_t timclk = tim_get_clock_hz_apb1();
  uint32_t presc = (timclk / 1000000u);
  if (presc == 0u) presc = 1u;
  presc -= 1u;

  tim->CR1 = 0;
  tim->PSC = (uint16_t)presc;
  tim->ARR = 20000u - 1u;
  tim->EGR = TIM_EGR_UG;
  tim->CR1 |= TIM_CR1_ARPE;
}

static void tim_config_pwm_channel(TIM_TypeDef* tim, uint8_t ch, uint16_t initial_us)
{
  if (ch == 1)
  {
    tim->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_OC1PE);
    tim->CCMR1 |= (6u << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE;
    tim->CCR1 = initial_us;
    tim->CCER |= TIM_CCER_CC1E;
  }
  else if (ch == 2)
  {
    tim->CCMR1 &= ~(TIM_CCMR1_OC2M | TIM_CCMR1_OC2PE);
    tim->CCMR1 |= (6u << TIM_CCMR1_OC2M_Pos) | TIM_CCMR1_OC2PE;
    tim->CCR2 = initial_us;
    tim->CCER |= TIM_CCER_CC2E;
  }
  else if (ch == 3)
  {
    tim->CCMR2 &= ~(TIM_CCMR2_OC3M | TIM_CCMR2_OC3PE);
    tim->CCMR2 |= (6u << TIM_CCMR2_OC3M_Pos) | TIM_CCMR2_OC3PE;
    tim->CCR3 = initial_us;
    tim->CCER |= TIM_CCER_CC3E;
  }
  else if (ch == 4)
  {
    tim->CCMR2 &= ~(TIM_CCMR2_OC4M | TIM_CCMR2_OC4PE);
    tim->CCMR2 |= (6u << TIM_CCMR2_OC4M_Pos) | TIM_CCMR2_OC4PE;
    tim->CCR4 = initial_us;
    tim->CCER |= TIM_CCER_CC4E;
  }

  tim->EGR = TIM_EGR_UG;
}

static void tim_start(TIM_TypeDef* tim)
{
  tim->CR1 |= TIM_CR1_CEN;
}

/* =========================
 *  Init once
 * ========================= */

static void init_internal_once(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  memset(s_ports, 0, sizeof(s_ports));
  for (uint8_t i = 0; i < SERVO_PORT_COUNT; i++)
  {
    s_ports[i].model_id = SERVO_MODEL_NONE;
    s_ports[i].current_pwm_us = 1500;
  }

  s_rx_inited = 0U;
  for (uint8_t i = 0; i < SERVO_CAN_COUNT; i++)
  {
    s_last_general[i] = -999999;
    s_last_pos_tgt[i] = -999999;
    s_last_vel_tgt[i] = -999999;
  }

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /* VCC enable pins: output PP */
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

  for (uint8_t i = 0; i < SERVO_PORT_COUNT; i++)
  {
    GPIO_InitStruct.Pin = s_hw[i].vcc_pin;
    HAL_GPIO_Init(s_hw[i].vcc_port, &GPIO_InitStruct);
    set_vcc(i, false);
  }

  /* Feedback pins: analog */
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
  GPIO_InitStruct.Pull = GPIO_NOPULL;

  for (uint8_t i = 0; i < SERVO_PORT_COUNT; i++)
  {
    if (s_hw[i].has_fbk_pin)
    {
      GPIO_InitStruct.Pin = s_hw[i].fbk_pin;
      HAL_GPIO_Init(s_hw[i].fbk_port, &GPIO_InitStruct);
    }
  }

  /* --- SANITY DRIVE BLOCK ---
   * Force PWM pins as plain GPIO outputs briefly.
   * If you scope the pin and it STILL only rises to ~0.3V here,
   * then the net is being clamped/shorted in hardware.
   */
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  for (uint8_t i = 0; i < SERVO_PORT_COUNT; i++)
  {
    GPIO_InitStruct.Pin = s_hw[i].pwm_pin;
    HAL_GPIO_Init(s_hw[i].pwm_port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(s_hw[i].pwm_port, s_hw[i].pwm_pin, GPIO_PIN_SET);
    for (volatile uint32_t d = 0; d < 50000; d++) { __NOP(); }

    HAL_GPIO_WritePin(s_hw[i].pwm_port, s_hw[i].pwm_pin, GPIO_PIN_RESET);
    for (volatile uint32_t d = 0; d < 50000; d++) { __NOP(); }
  }

  /* PWM pins: AF push-pull, VERY_HIGH speed */
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL; /* change to PULLUP if your buffer needs it */
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;

  for (uint8_t i = 0; i < SERVO_PORT_COUNT; i++)
  {
    GPIO_InitStruct.Pin       = s_hw[i].pwm_pin;
    GPIO_InitStruct.Alternate = s_hw[i].pwm_af;
    HAL_GPIO_Init(s_hw[i].pwm_port, &GPIO_InitStruct);
  }

  /* TIM setup */
  tim_init_1mhz_50hz(TIM2);
  tim_init_1mhz_50hz(TIM3);
  tim_init_1mhz_50hz(TIM4);

  tim_config_pwm_channel(TIM2, 1, 1500);
  tim_config_pwm_channel(TIM2, 2, 1500);
  tim_config_pwm_channel(TIM2, 4, 1500);

  tim_config_pwm_channel(TIM3, 1, 1500);
  tim_config_pwm_channel(TIM3, 2, 1500);
  tim_config_pwm_channel(TIM3, 3, 1500);

  tim_config_pwm_channel(TIM4, 2, 1500);
  tim_config_pwm_channel(TIM4, 3, 1500);

  tim_start(TIM2);
  tim_start(TIM3);
  tim_start(TIM4);

  /* Default ports 0..5 to HS-645MG and turn on VCC */
  for (uint8_t p = 0; p < SERVO_CAN_COUNT; p++)
  {
    s_ports[p].model_id = SERVO_MODEL_HS_645MG;
    set_vcc(p, true);
    set_pwm_us(p, s_servo_defs[SERVO_MODEL_HS_645MG].pwm_min_us);
  }
}

/* =========================
 *  Actions
 * ========================= */

static void stop_motor(uint8_t port)
{
  (void)port;
}

static void publish_vectors(uint8_t port)
{
  if (port >= SERVO_CAN_COUNT) return;

  const ServoDef_t* def = get_def(s_ports[port].model_id);

  int32_t pos_out = -1;
  int32_t vel_out = -1;

  if (def != NULL)
  {
    (void)compute_position_from_pwm(def, s_ports[port].current_pwm_us, &pos_out);
  }

  (void)CanSystem_SetInt32(s_can_pos_out[port], pos_out);
  (void)CanSystem_SetInt32(s_can_vel_out[port], vel_out);
}

/* =========================
 *  Public API
 * ========================= */

bool ServoSystem_SetServoModel(uint8_t port, uint8_t model_id)
{
  if (!s_inited)
  {
    s_inited = 1U;
    init_internal_once();
  }

  if (!is_port_valid(port)) return false;

  const ServoDef_t* def = get_def(model_id);
  if (def == NULL) return false;

  s_ports[port].model_id = model_id;

  if (model_id == SERVO_MODEL_NONE)
  {
    set_vcc(port, false);
    set_pwm_us(port, 1500);
    return true;
  }

  set_vcc(port, true);

  if (def->modes.position)
    set_pwm_us(port, def->pwm_min_us);
  else
    set_pwm_us(port, 1500);

  return true;
}

uint8_t ServoSystem_GetServoModel(uint8_t port)
{
  if (!is_port_valid(port)) return SERVO_MODEL_NONE;
  return s_ports[port].model_id;
}

bool ServoSystem_SetPositionDeg(uint8_t port, float position_deg)
{
  if (!s_inited)
  {
    s_inited = 1U;
    init_internal_once();
  }

  if (!is_port_valid(port)) return false;

  const ServoDef_t* def = get_def(s_ports[port].model_id);
  if ((def == NULL) || !def->modes.position) return false;

  uint16_t pwm_us = 0;
  if (!compute_pwm_us_for_position(def, position_deg, &pwm_us))
    return false;

  /* Always ensure power is on when commanding */
  set_vcc(port, true);

  s_ports[port].target_position_deg = position_deg;
  set_pwm_us(port, pwm_us);
  return true;
}

bool ServoSystem_SetVelocityDegS(uint8_t port, float velocity_deg_s)
{
  (void)port;
  (void)velocity_deg_s;
  return false;
}

void ServoSystem_Controller(void)
{
  if (!s_inited)
  {
    s_inited = 1U;
    init_internal_once();
  }

  for (uint8_t i = 0; i < SERVO_CAN_COUNT; i++)
  {
    int32_t gen = 0;
    if (CanParams_GetInt32(s_can_general[i], &gen))
    {
      if (!s_rx_inited || (gen != s_last_general[i]))
      {
        s_last_general[i] = gen;

        switch ((uint8_t)gen)
        {
          case 0: ServoSystem_OnSetZero(i); break;
          case 1: ServoSystem_OnRequestVectors(i); publish_vectors(i); break;
          case 2: ServoSystem_OnStopMotor(i); stop_motor(i); break;
          case 3: ServoSystem_OnShutdownMotor(i); set_vcc(i, false); break;
          case 4: ServoSystem_OnClearErrors(i); break;
          default: break;
        }
      }
    }

    int32_t pos_tgt = 0;
    if (CanParams_GetInt32(s_can_pos_tgt[i], &pos_tgt))
    {
      if (!s_rx_inited || (pos_tgt != s_last_pos_tgt[i]))
      {
        s_last_pos_tgt[i] = pos_tgt;
        (void)ServoSystem_SetPositionDeg(i, (float)pos_tgt);
      }
    }

    int32_t vel_tgt = 0;
    if (CanParams_GetInt32(s_can_vel_tgt[i], &vel_tgt))
    {
      if (!s_rx_inited || (vel_tgt != s_last_vel_tgt[i]))
      {
        s_last_vel_tgt[i] = vel_tgt;
        (void)ServoSystem_SetVelocityDegS(i, (float)vel_tgt);
      }
    }
  }

  s_rx_inited = 1U;
}

void servo_system_controller(void)
{
  ServoSystem_Controller();
}
