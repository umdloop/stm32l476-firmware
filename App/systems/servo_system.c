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
  uint8_t      type;

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
    .type = SERVO_TYPE_UNDEFINED,

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
    .type = SERVO_TYPE_STANDARD,

    .pwm_min_us = 553,
    .pwm_max_us = 2520,

    .max_rotation_deg = 197.0f,
    .max_diff_position_deg = 197.0f,

    .travel_deg_per_us = 0.100f,

    .vel_neutral_us = 1500,
    .vel_deg_s_per_us = 0.0f,

    .max_speed_deg_s = 250.0f,

    .feedback = { .has_feedback = false },
  },
  {
    .name = "DFRobot Dual Mode",
    .modes = { .position = true, .velocity = false },
    .type = SERVO_TYPE_STANDARD,

    .pwm_min_us = 500,
    .pwm_max_us = 2500,

    .max_rotation_deg = 360.0f,
    .max_diff_position_deg = 360.0f,

    .travel_deg_per_us = 0.180f,

    .vel_neutral_us = 1500,
    .vel_deg_s_per_us = 0.0f,

    .max_speed_deg_s = 252.0f,

    .feedback = { .has_feedback = false },
  },
  {
    .name = "GoBilda Dual Mode 5-Turn",
    .modes = { .position = true, .velocity = false },
    .type = SERVO_TYPE_STANDARD,

    .pwm_min_us = 500,
    .pwm_max_us = 2500,

    .max_rotation_deg = 1800.0f,
    .max_diff_position_deg = 1800.0f,

    .travel_deg_per_us = 0.9f,

    .vel_neutral_us = 1500,
    .vel_deg_s_per_us = 0.0f,

    .max_speed_deg_s = 690.0f,

    .feedback = { .has_feedback = false },
  },
  {
    .name  = "HiTec HS-5055MG",
    .modes = { .position = true, .velocity = false },
    .type = SERVO_TYPE_STANDARD,

    .pwm_min_us = 900,
    .pwm_max_us = 2100,

    .max_rotation_deg = 125.0f,
    .max_diff_position_deg = 125.0f,

    .travel_deg_per_us = 0.10f,

    .vel_neutral_us = 1500,
    .vel_deg_s_per_us = 0.0f,

    .max_speed_deg_s = 352.0f,

    .feedback = { .has_feedback = false },
  },
  {
    .name = "FeeTech FT6335M",
    .modes = { .position = true, .velocity = false },
    .type = SERVO_TYPE_STANDARD,

    .pwm_min_us = 500,
    .pwm_max_us = 2500,

    .max_rotation_deg = 360.0f,
    .max_diff_position_deg = 360.0f,

    .travel_deg_per_us = 0.18f,

    .vel_neutral_us = 1500,
    .vel_deg_s_per_us = 0.0f,

    .max_speed_deg_s = 312.5f,  // ~0.12s/60° equivalent

    .feedback = { .has_feedback = false }
  },
  {
      .name = "MG-90S",
      .modes = { .position = true, .velocity = false },
      .type = SERVO_TYPE_STANDARD,

      .pwm_min_us = 1000,
      .pwm_max_us = 2000,

      .max_rotation_deg = 150.0f,
      .max_diff_position_deg = 150.0f,

      .travel_deg_per_us = 0.15f,

      .vel_neutral_us = 1500,
      .vel_deg_s_per_us = 0.0f,

      .max_speed_deg_s = 300.0f,  // 50 RPM

      .feedback = { .has_feedback = false }
  },
  {
    .name = "GoBilda Dual Mode Standard",
    .modes = { .position = true, .velocity = false },
    .type = SERVO_TYPE_STANDARD,

    .pwm_min_us = 500,
    .pwm_max_us = 2500,

    .max_rotation_deg = 300.0f,
    .max_diff_position_deg = 300.0f,

    .travel_deg_per_us = 0.15f,

    .vel_neutral_us = 1500,
    .vel_deg_s_per_us = 0.5f,

    .max_speed_deg_s = 300.0f,  // 50 RPM

    .feedback = { .has_feedback = false },
  },
  {
    .name = "GoBilda Dual Mode Continuous",
    .modes = { .position = false, .velocity = true },
    .type = SERVO_TYPE_CONTINUOUS,

    .pwm_min_us = 900,
    .pwm_max_us = 2100,

    .max_rotation_deg = 360.0f,
    .max_diff_position_deg = 360.0f,

    .travel_deg_per_us = 0.15f, // irrelevant

    .vel_neutral_us = 1500,
    .vel_deg_s_per_us = 0.6f, // 50 RPM -> 300 deg/s -> 500 us either direction -> 300/500 = 0.6

    .max_speed_deg_s = 300.0f,

    .feedback = { .has_feedback = false },
  },
  { // EDIT Zed Servo PCB
      .name = "DSS-M15S",
      .modes = { .position = true, .velocity = false },
      .type = SERVO_TYPE_STANDARD,

      .pwm_min_us = 500,
      .pwm_max_us = 2500,

      .max_rotation_deg = 270.0f,
      .max_diff_position_deg = 270.0f,

      .travel_deg_per_us = 0.135f,

      .vel_neutral_us = 1500,
      .vel_deg_s_per_us = 0.0f,

      .max_speed_deg_s = 250.0f,

      .feedback = { .has_feedback = false },
    },
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
		// IGNORE GPIOB, GPIO_PIN_15
  { GPIOC, GPIO_PIN_8,  GPIO_AF2_TIM3, TIM3, 3, GPIOB, GPIO_PIN_15, NULL, 0, false },
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

#define SERVO_CAN_COUNT (1u)

static const char* s_can_pos_tgt[SERVO_CAN_COUNT] =
{
  "SWERVE_PCB_C.servo_position_target_0",
};

static const char* s_can_vel_tgt[SERVO_CAN_COUNT] =
{
  "SWERVE_PCB_C.servo_velocity_target_0",
};

static const char* s_can_mot_state_req[SERVO_CAN_COUNT] =
{
  "SWERVE_PCB_C.servo_state_req_event_0",
};

static const char* s_can_mot_status_req[SERVO_CAN_COUNT] =
{
  "SWERVE_PCB_C.servo_status_req_event_0",
};

static const char* s_can_maint_cmd[SERVO_CAN_COUNT] =
{
  "SWERVE_PCB_C.servo_maintenance_cmd_0",
};

static const char* s_can_spec_req[SERVO_CAN_COUNT] =
{
  "SWERVE_PCB_C.servo_spec_req_event_0",
};

static const char* s_can_pos_out[SERVO_CAN_COUNT][3] =
{
  // Servo 0
  {
    "SWERVE_PCB_R.servo_position_pos_resp_0", // Position Command Response
    "SWERVE_PCB_R.servo_position_vel_resp_0", // Velocity Command Response
    "SWERVE_PCB_R.servo_position_state_resp_0" // Motor State Command Response
  },
};

static const char* s_can_vel_out[SERVO_CAN_COUNT][3] =
{
  // Servo 0
  {
    "SWERVE_PCB_R.servo_velocity_pos_resp_0", // Position Command Response
    "SWERVE_PCB_R.servo_velocity_vel_resp_0", // Velocity Command Response
    "SWERVE_PCB_R.servo_velocity_state_resp_0" // Motor State Command Response
  },
};

static const char* s_can_motor_status[SERVO_CAN_COUNT] =
{
  "SWERVE_PCB_R.servo_status_0",
};

static const char* s_can_maint_succ[SERVO_CAN_COUNT] =
{
  "SWERVE_PCB_R.servo_maintenance_success_0",
};

static const char* s_can_servo_type[SERVO_CAN_COUNT] =
{
  "SWERVE_PCB_R.servo_type_0",
};

static const char* s_can_pos_max[SERVO_CAN_COUNT] =
{
  "SWERVE_PCB_R.servo_position_max_0",
};

static const char* s_can_vel_max[SERVO_CAN_COUNT] =
{
  "SWERVE_PCB_R.servo_velocity_max_0",
};

static uint8_t s_rx_inited = 0U;
static int32_t s_last_pos_tgt[SERVO_CAN_COUNT];
static int32_t s_last_vel_tgt[SERVO_CAN_COUNT];
static int32_t s_last_state_req[SERVO_CAN_COUNT];
static int32_t s_last_status_req[SERVO_CAN_COUNT];
static int32_t s_last_maint_req[SERVO_CAN_COUNT];
static int32_t s_last_spec_req[SERVO_CAN_COUNT];

/* Weak callbacks */
__attribute__((weak)) void ServoSystem_OnSetZero(uint8_t port)        { (void)port; }
__attribute__((weak)) void ServoSystem_OnRequestVectors(uint8_t port) { (void)port; }
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

/* Velocity -> PWM */
static bool compute_pwm_us_for_velocity(const ServoDef_t* def, float velocity_deg_s, uint16_t* out_pwm_us)
{
  if ((def == NULL) || (out_pwm_us == NULL)) return false;
  if (!def->modes.velocity) return false;

  velocity_deg_s = clampf(velocity_deg_s, -1*(def->max_speed_deg_s), def->max_speed_deg_s);

  float us = (velocity_deg_s / def->vel_deg_s_per_us) + (float)def->vel_neutral_us;
  us = clampf(us, (float)def->pwm_min_us, (float)def->pwm_max_us);

  *out_pwm_us = (uint16_t)(us + 0.5f);
  return true;
}

/* PWM -> velocity estimate */
static bool compute_velocity_from_pwm(const ServoDef_t* def, uint16_t pwm_us, int32_t* out_vel_deg)
{
  if ((def == NULL) || (out_vel_deg == NULL)) return false;
  if (!def->modes.velocity) return false;

  float vel = ((float)pwm_us - (float)def->pwm_min_us) * def->vel_deg_s_per_us;
  vel = clampf(vel, 0.0f, def->max_rotation_deg);

  *out_vel_deg = (int32_t)(vel + 0.5f);
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
    s_last_pos_tgt[i] = -999999;
    s_last_vel_tgt[i] = -999999;
    s_last_state_req[i] = -999999;
    s_last_status_req[i] = -999999;
    s_last_maint_req[i] = -999999;
    s_last_spec_req[i] = -999999;
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

  /* Default ports 0..8 to GOBILDA 5 Turn and turn on VCC */
//  for (uint8_t p = 0; p < SERVO_CAN_COUNT; p++)
//  {
//    s_ports[p].model_id = SERVO_MODEL_GOBILDA_STANDARD;
//    set_vcc(p, false);
//    set_pwm_us(p, s_servo_defs[SERVO_MODEL_GOBILDA_STANDARD].pwm_min_us);
//  }
//
//  s_ports[5].model_id = SERVO_MODEL_GOBILDA_CONTINUOUS;
//  set_pwm_us(5, s_servo_defs[SERVO_MODEL_GOBILDA_CONTINUOUS].vel_neutral_us);

  s_ports[0].model_id = SERVO_MODEL_DSS_M15S;

  set_vcc(0, false);
  set_pwm_us(0, s_servo_defs[SERVO_MODEL_DSS_M15S].vel_neutral_us);

  /*
  // TESTING
  // Port 0, which is technically port 6 does not work
  // Overwriting port 1 with GOBilda servo
  s_ports[1].model_id = SERVO_MODEL_GOBILDA_5TURN;
  set_vcc(1, true);
  set_pwm_us(1, s_servo_defs[SERVO_MODEL_GOBILDA_5TURN].pwm_min_us);

  // Overwriting port 2 with GOBilda servo
  s_ports[2].model_id = SERVO_MODEL_GOBILDA_5TURN;
  set_vcc(2, true);
  set_pwm_us(2, s_servo_defs[SERVO_MODEL_GOBILDA_5TURN].pwm_min_us);

  // Overwriting port 3 with HS_5055MG servo
  s_ports[3].model_id = SERVO_MODEL_HS_5055MG;
  set_vcc(3, true);
  set_pwm_us(3, s_servo_defs[SERVO_MODEL_HS_5055MG].pwm_min_us);

  // Overwriting port 5 with HS_5055MG servo
  s_ports[5].model_id = SERVO_MODEL_HS_5055MG;
  set_vcc(5, true);
  set_pwm_us(5, s_servo_defs[SERVO_MODEL_HS_5055MG].pwm_min_us);

  // Overwriting port 6, which is technically 0, is FT_6335M servo
  s_ports[0].model_id = SERVO_MODEL_FT_6335M;
  set_vcc(0, true);
  set_pwm_us(0, s_servo_defs[SERVO_MODEL_FT_6335M].pwm_min_us);
  */

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
    if(def->modes.position){
      (void)compute_position_from_pwm(def, s_ports[port].current_pwm_us, &pos_out);
    }
    if(def->modes.velocity){
      (void)compute_velocity_from_pwm(def, s_ports[port].current_pwm_us, &vel_out);
    }
  }

  for (int i = 0; i < 3; i++)
  {
    (void)CanParams_SetInt32(s_can_pos_out[port][i], pos_out);
    (void)CanParams_SetInt32(s_can_vel_out[port][i], vel_out);
  }
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

uint8_t ServoSystem_GetServoType(uint8_t port)
{
  if (!is_port_valid(port)) return 0;
  const ServoDef_t* def = get_def(s_ports[port].model_id);
  if (def == NULL) return 0;
  return (uint8_t)(def->type);
}

uint16_t ServoSystem_GetPosMax(uint8_t port)
{
  if (!is_port_valid(port)) return 0;
  const ServoDef_t* def = get_def(s_ports[port].model_id);
  if (def == NULL) return 0;
  return (uint16_t)(def->max_rotation_deg);
}

uint16_t ServoSystem_GetVelMax(uint8_t port)
{
  if (!is_port_valid(port)) return 0;
  const ServoDef_t* def = get_def(s_ports[port].model_id);
  if (def == NULL) return 0;
  return (uint16_t)(def->max_speed_deg_s);
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

  if (pwm_us == s_ports[port].current_pwm_us)
	  return false;
  /* Always ensure power is on when commanding */
  set_vcc(port, true);

  s_ports[port].target_position_deg = position_deg;
  set_pwm_us(port, pwm_us);
  return true;
}

bool ServoSystem_SetVelocityDegS(uint8_t port, float velocity_deg_s)
{
  if (!s_inited)
  {
    s_inited = 1U;
    init_internal_once();
  }

  if (!is_port_valid(port)) return false;

  const ServoDef_t* def = get_def(s_ports[port].model_id);
  if ((def == NULL) || !def->modes.velocity) return false;

  uint16_t pwm_us = 0;
  if (!compute_pwm_us_for_velocity(def, velocity_deg_s, &pwm_us))
    return false;

  if (pwm_us == s_ports[port].current_pwm_us)
	  return false;

  /* Always ensure power is on when commanding */
  set_vcc(port, true);

  s_ports[port].target_velocity_deg_s = velocity_deg_s;
  set_pwm_us(port, pwm_us);
  return true;
}

void ServoSystem_OnMotorStatusCmd(uint8_t port)
{
  if (!is_port_valid(port)) return;
  GPIO_PinState state = HAL_GPIO_ReadPin(s_hw[port].vcc_port, s_hw[port].vcc_pin);
  const ServoDef_t* def = get_def(s_ports[port].model_id);

  // TODO: COMPLETE THIS WITH EVERYTHING ELSE
  if (state == GPIO_PIN_RESET) {
    (void)CanParams_SetInt32(s_can_motor_status[port], MOTOR_STATUS_STOPPED);
    (void)CanSystem_Send(s_can_motor_status[port]);
  }
  else if(def->type == SERVO_TYPE_CONTINUOUS && state != GPIO_PIN_RESET){
    (void)CanParams_SetInt32(s_can_motor_status[port], MOTOR_STATUS_VELOCITY_CONTROL);
    (void)CanSystem_Send(s_can_motor_status[port]);
  }
  else if(def->type == SERVO_TYPE_STANDARD && state != GPIO_PIN_RESET){
    (void)CanParams_SetInt32(s_can_motor_status[port], MOTOR_STATUS_POSITION_CONTROL);
    (void)CanSystem_Send(s_can_motor_status[port]);
  }
  else if(def->type == SERVO_TYPE_UNDEFINED && state != GPIO_PIN_RESET){
    (void)CanParams_SetInt32(s_can_motor_status[port], MOTOR_STATUS_IDLE);
    (void)CanSystem_Send(s_can_motor_status[port]);
  }
  else{
    (void)CanParams_SetInt32(s_can_motor_status[port], MOTOR_STATUS_UNDEFINED);
    (void)CanSystem_Send(s_can_motor_status[port]);
  }
}

void ServoSystem_OnStopMotor(uint8_t port){
	stop_motor(port);
}

void ServoSystem_OnShutdownMotor(uint8_t port){
	set_vcc(port, false);
}

void ServoSystem_OnMotorSpecCmd(uint8_t port)
{
  if (!is_port_valid(port)) return;

  const ServoDef_t* def = get_def(s_ports[port].model_id);

  (void)CanParams_SetInt32(s_can_servo_type[port], def->type);
  (void)CanParams_SetInt32(s_can_pos_max[port], def->max_rotation_deg);
  (void)CanParams_SetInt32(s_can_vel_max[port], def->max_speed_deg_s);

  (void)CanSystem_Send(s_can_servo_type[port]);
  (void)CanSystem_Send(s_can_pos_max[port]);
  (void)CanSystem_Send(s_can_vel_max[port]);
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
    // Position Command Frame
    int32_t pos_tgt = 0;

	if (CanParams_GetInt32(s_can_pos_tgt[i], &pos_tgt))
	{
	  if (s_rx_inited)
	  {
		s_last_pos_tgt[i] = pos_tgt;
		(void)ServoSystem_SetPositionDeg(i, (float)pos_tgt);
	  }
	}


    // Velocity Command Frame
    int32_t vel_tgt = 0;

	if (CanParams_GetInt32(s_can_vel_tgt[i], &vel_tgt))
	{
	  if (s_rx_inited)
	  {
		s_last_vel_tgt[i] = vel_tgt;
		(void)ServoSystem_SetVelocityDegS(i, (float)vel_tgt);
	  }
	}


    // Motor Status Frame
    int32_t status_req = 0;

	if (CanParams_GetInt32(s_can_mot_status_req[i], &status_req))
	{
	  if (s_rx_inited && (status_req != s_last_status_req[i]))
	  {
		s_last_status_req[i] = status_req;
		if(status_req == 1)
		{
		  ServoSystem_OnMotorStatusCmd(i);
		  s_last_status_req[i] = 0;
		}
	  }
	}


    // Motor State Frame
    int32_t state_req = 0;

	if (CanParams_GetInt32(s_can_mot_state_req[i], &state_req))
	{

	  if (s_rx_inited && (state_req != s_last_state_req[i]))
	  {
		s_last_state_req[i] = state_req;
		if(state_req == 1)
		{
		  publish_vectors(i);
		  s_last_state_req[i] = 0;
		}
	  }
	}


    // Maintenance Frame
    int32_t maint = 0;

	if (CanParams_GetInt32(s_can_maint_cmd[i], &maint))
	{
	  if (s_rx_inited && (maint != s_last_maint_req[i]))
	  {
		s_last_maint_req[i] = maint;
		switch ((uint8_t)maint)
		{
		  case 0: (void)CanParams_SetInt32(s_can_maint_succ[i], 1); (void)CanSystem_Send(s_can_maint_succ[i]); break;
		  case 1: ServoSystem_OnStopMotor(i); (void)CanParams_SetInt32(s_can_maint_succ[i], 1); (void)CanSystem_Send(s_can_maint_succ[i]); break;
		  case 2: ServoSystem_OnShutdownMotor(i); (void)CanParams_SetInt32(s_can_maint_succ[i], 1); (void)CanSystem_Send(s_can_maint_succ[i]); break;
		  case 3: ServoSystem_OnClearErrors(i); (void)CanParams_SetInt32(s_can_maint_succ[i], 1); (void)CanSystem_Send(s_can_maint_succ[i]); break;
		  case 4: ServoSystem_OnSetZero(i); (void)CanParams_SetInt32(s_can_maint_succ[i], 1); (void)CanSystem_Send(s_can_maint_succ[i]); break;
		  default: (void)CanParams_SetInt32(s_can_maint_succ[i], 0); (void)CanSystem_Send(s_can_maint_succ[i]); break;
		}
	  }
	}


    // Servo Specifications Frame
    int32_t spec_req = 0;
	if (CanParams_GetInt32(s_can_spec_req[i], &spec_req))
	{
	  if (s_rx_inited && (spec_req != s_last_spec_req[i]))
	  {
		s_last_spec_req[i] = spec_req;
		if ((uint8_t)spec_req)
		{
		  ServoSystem_OnMotorSpecCmd(i);
		  s_last_spec_req[i] = 0;
		}
	  }
	}

  }
  s_rx_inited = 1U;
}

void servo_system_controller(void)
{
  ServoSystem_Controller();
}
