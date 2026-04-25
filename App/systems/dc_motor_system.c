#include "dc_motor_system.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "stm32l4xx_hal.h"
#include "can_params.h"

/*
 * ==========================================================================
 *  User configuration
 * ==========================================================================
 */

#define DC_MOTOR_SYSTEM_NUM_MOTORS             2U

#define DC_MOTOR_SYSTEM_PCB_CAN_ID             0x70U

#define DC_MOTOR_SYSTEM_COMMAND_MESSAGE_NAME   "SCIENCE_DC_MOTOR_PCB_C"

/*
 * Max motor velocity in deg/s.
 *
 * abs(command) >= this value -> 100% PWM duty
 * command == 0               -> 0% PWM duty
 * otherwise                  -> linear duty scaling
 */
#define DC_MOTOR_SYSTEM_MAX_VELOCITY_DEG_S     1000U

/*
 * L298N PWM frequency.
 *
 * 1000 Hz is easy to verify on a scope.
 * You can raise this later if the motor/driver sounds bad.
 */
#define DC_MOTOR_SYSTEM_PWM_FREQUENCY_HZ       1000U

/*
 * How often to read the latest CAN velocity command from CanParams.
 */
#define DC_MOTOR_SYSTEM_COMMAND_PERIOD_MS      5U

/*
 * Configurable motor port IDs.
 *
 * These are the low nibble of the command byte:
 *
 *   0x30 + DC_MOTOR_SYSTEM_MOTOR_A_CMD_ID
 *   0x30 + DC_MOTOR_SYSTEM_MOTOR_B_CMD_ID
 *
 * Valid range: 0x0 to 0xF
 */
#define DC_MOTOR_SYSTEM_MOTOR_A_CMD_ID         0x0U
#define DC_MOTOR_SYSTEM_MOTOR_B_CMD_ID         0x1U

/*
 * Default L298N pinout.
 *
 * Motor A:
 *   IN1 = PC6
 *   IN2 = PB12
 *   ENA = PB15
 *
 * Motor B:
 *   IN3 = PB2
 *   IN4 = PB0
 *   ENB = PB11
 */

/* Motor A direction pins */
#define DC_MOTOR_SYSTEM_IN1_PORT               GPIOC
#define DC_MOTOR_SYSTEM_IN1_PIN                GPIO_PIN_6

#define DC_MOTOR_SYSTEM_IN2_PORT               GPIOB
#define DC_MOTOR_SYSTEM_IN2_PIN                GPIO_PIN_12

/* Motor A enable PWM pin: PB15 -> TIM15_CH2 AF14 */
#define DC_MOTOR_SYSTEM_ENA_PORT               GPIOB
#define DC_MOTOR_SYSTEM_ENA_PIN                GPIO_PIN_15
#define DC_MOTOR_SYSTEM_ENA_AF                 GPIO_AF14_TIM15
#define DC_MOTOR_SYSTEM_ENA_TIMER              TIM15
#define DC_MOTOR_SYSTEM_ENA_CHANNEL            2U

/* Motor B direction pins */
#define DC_MOTOR_SYSTEM_IN3_PORT               GPIOB
#define DC_MOTOR_SYSTEM_IN3_PIN                GPIO_PIN_2

#define DC_MOTOR_SYSTEM_IN4_PORT               GPIOB
#define DC_MOTOR_SYSTEM_IN4_PIN                GPIO_PIN_0

/* Motor B enable PWM pin: PB11 -> TIM2_CH4 AF1 */
#define DC_MOTOR_SYSTEM_ENB_PORT               GPIOB
#define DC_MOTOR_SYSTEM_ENB_PIN                GPIO_PIN_11
#define DC_MOTOR_SYSTEM_ENB_AF                 GPIO_AF1_TIM2
#define DC_MOTOR_SYSTEM_ENB_TIMER              TIM2
#define DC_MOTOR_SYSTEM_ENB_CHANNEL            4U

#if (DC_MOTOR_SYSTEM_NUM_MOTORS < 1U) || (DC_MOTOR_SYSTEM_NUM_MOTORS > 2U)
#error "DC_MOTOR_SYSTEM_NUM_MOTORS must be 1 or 2."
#endif

#if (DC_MOTOR_SYSTEM_MAX_VELOCITY_DEG_S == 0U)
#error "DC_MOTOR_SYSTEM_MAX_VELOCITY_DEG_S must be greater than 0."
#endif

#if (DC_MOTOR_SYSTEM_PWM_FREQUENCY_HZ == 0U)
#error "DC_MOTOR_SYSTEM_PWM_FREQUENCY_HZ must be greater than 0."
#endif

/*
 * ==========================================================================
 *  Private types
 * ==========================================================================
 */

typedef struct
{
  GPIO_TypeDef* in_forward_port;
  uint16_t      in_forward_pin;

  GPIO_TypeDef* in_reverse_port;
  uint16_t      in_reverse_pin;

  GPIO_TypeDef* enable_port;
  uint16_t      enable_pin;
  uint32_t      enable_af;

  TIM_TypeDef*  pwm_timer;
  uint8_t       pwm_channel;

  uint8_t       command_id;
  const char*   velocity_param_name;

  int32_t       target_velocity_deg_s;
  uint16_t      duty_per_mille;
} DcMotor_t;

/*
 * ==========================================================================
 *  Private state
 * ==========================================================================
 */

static bool s_initialized = false;
static uint32_t s_last_command_tick_ms = 0U;

static bool s_tim2_initialized = false;
static bool s_tim15_initialized = false;

static DcMotor_t s_motors[DC_MOTOR_SYSTEM_NUM_MOTORS] =
{
#if (DC_MOTOR_SYSTEM_NUM_MOTORS >= 1U)
  {
    .in_forward_port = DC_MOTOR_SYSTEM_IN1_PORT,
    .in_forward_pin  = DC_MOTOR_SYSTEM_IN1_PIN,

    .in_reverse_port = DC_MOTOR_SYSTEM_IN2_PORT,
    .in_reverse_pin  = DC_MOTOR_SYSTEM_IN2_PIN,

    .enable_port     = DC_MOTOR_SYSTEM_ENA_PORT,
    .enable_pin      = DC_MOTOR_SYSTEM_ENA_PIN,
    .enable_af       = DC_MOTOR_SYSTEM_ENA_AF,

    .pwm_timer       = DC_MOTOR_SYSTEM_ENA_TIMER,
    .pwm_channel     = DC_MOTOR_SYSTEM_ENA_CHANNEL,

    .command_id      = DC_MOTOR_SYSTEM_MOTOR_A_CMD_ID,
    .velocity_param_name = NULL,

    .target_velocity_deg_s = 0,
    .duty_per_mille = 0U,
  },
#endif

#if (DC_MOTOR_SYSTEM_NUM_MOTORS >= 2U)
  {
    .in_forward_port = DC_MOTOR_SYSTEM_IN3_PORT,
    .in_forward_pin  = DC_MOTOR_SYSTEM_IN3_PIN,

    .in_reverse_port = DC_MOTOR_SYSTEM_IN4_PORT,
    .in_reverse_pin  = DC_MOTOR_SYSTEM_IN4_PIN,

    .enable_port     = DC_MOTOR_SYSTEM_ENB_PORT,
    .enable_pin      = DC_MOTOR_SYSTEM_ENB_PIN,
    .enable_af       = DC_MOTOR_SYSTEM_ENB_AF,

    .pwm_timer       = DC_MOTOR_SYSTEM_ENB_TIMER,
    .pwm_channel     = DC_MOTOR_SYSTEM_ENB_CHANNEL,

    .command_id      = DC_MOTOR_SYSTEM_MOTOR_B_CMD_ID,
    .velocity_param_name = NULL,

    .target_velocity_deg_s = 0,
    .duty_per_mille = 0U,
  },
#endif
};

static char s_velocity_param_names[DC_MOTOR_SYSTEM_NUM_MOTORS][80];

/*
 * ==========================================================================
 *  GPIO helpers
 * ==========================================================================
 */

static void enable_gpio_clock(GPIO_TypeDef* port)
{
  if (port == GPIOA)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
  }
  else if (port == GPIOB)
  {
    __HAL_RCC_GPIOB_CLK_ENABLE();
  }
  else if (port == GPIOC)
  {
    __HAL_RCC_GPIOC_CLK_ENABLE();
  }
  else if (port == GPIOD)
  {
    __HAL_RCC_GPIOD_CLK_ENABLE();
  }
  else if (port == GPIOE)
  {
    __HAL_RCC_GPIOE_CLK_ENABLE();
  }
  else if (port == GPIOH)
  {
    __HAL_RCC_GPIOH_CLK_ENABLE();
  }
}

static void gpio_init_output(GPIO_TypeDef* port, uint16_t pin)
{
  GPIO_InitTypeDef gpio = {0};

  enable_gpio_clock(port);

  HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);

  gpio.Pin = pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;

  HAL_GPIO_Init(port, &gpio);
}

static void gpio_init_pwm_af(GPIO_TypeDef* port, uint16_t pin, uint32_t alternate_function)
{
  GPIO_InitTypeDef gpio = {0};

  enable_gpio_clock(port);

  gpio.Pin = pin;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = alternate_function;

  HAL_GPIO_Init(port, &gpio);
}

/*
 * ==========================================================================
 *  Timer PWM helpers
 * ==========================================================================
 */

static void enable_timer_clock(TIM_TypeDef* timer)
{
  if (timer == TIM2)
  {
    __HAL_RCC_TIM2_CLK_ENABLE();
  }
  else if (timer == TIM15)
  {
    __HAL_RCC_TIM15_CLK_ENABLE();
  }
}

static uint32_t get_apb1_timer_clock_hz(void)
{
  uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
  uint32_t ppre1 = RCC->CFGR & RCC_CFGR_PPRE1;

  if (ppre1 == RCC_CFGR_PPRE1_DIV1)
  {
    return pclk1;
  }

  return pclk1 * 2U;
}

static uint32_t get_apb2_timer_clock_hz(void)
{
  uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
  uint32_t ppre2 = RCC->CFGR & RCC_CFGR_PPRE2;

  if (ppre2 == RCC_CFGR_PPRE2_DIV1)
  {
    return pclk2;
  }

  return pclk2 * 2U;
}

static uint32_t get_timer_clock_hz(TIM_TypeDef* timer)
{
  if (timer == TIM2)
  {
    return get_apb1_timer_clock_hz();
  }

  if (timer == TIM15)
  {
    return get_apb2_timer_clock_hz();
  }

  return 0U;
}

static bool timer_already_initialized(TIM_TypeDef* timer)
{
  if (timer == TIM2)
  {
    return s_tim2_initialized;
  }

  if (timer == TIM15)
  {
    return s_tim15_initialized;
  }

  return false;
}

static void mark_timer_initialized(TIM_TypeDef* timer)
{
  if (timer == TIM2)
  {
    s_tim2_initialized = true;
  }
  else if (timer == TIM15)
  {
    s_tim15_initialized = true;
  }
}

static bool pwm_timer_base_init(TIM_TypeDef* timer)
{
  uint32_t timer_clock_hz = 0U;
  uint32_t prescaler = 0U;
  uint32_t arr = 0U;

  if (timer == NULL)
  {
    return false;
  }

  if (timer_already_initialized(timer))
  {
    return true;
  }

  timer_clock_hz = get_timer_clock_hz(timer);
  if (timer_clock_hz == 0U)
  {
    return false;
  }

  enable_timer_clock(timer);

  /*
   * Configure timer tick to 1 MHz.
   * Then ARR gives period in microseconds.
   *
   * Example:
   *   1000 Hz PWM -> ARR = 999 -> 1000 timer counts per PWM period.
   */
  prescaler = timer_clock_hz / 1000000U;
  if (prescaler == 0U)
  {
    prescaler = 1U;
  }
  prescaler -= 1U;

  arr = (1000000U / DC_MOTOR_SYSTEM_PWM_FREQUENCY_HZ);
  if (arr == 0U)
  {
    arr = 1U;
  }
  arr -= 1U;

  timer->CR1 = 0U;
  timer->PSC = (uint16_t)prescaler;
  timer->ARR = (uint32_t)arr;
  timer->CNT = 0U;

  timer->EGR = TIM_EGR_UG;
  timer->CR1 |= TIM_CR1_ARPE;

  /*
   * TIM15 is an advanced-control timer, so main output enable is required.
   */
  if (timer == TIM15)
  {
    timer->BDTR |= TIM_BDTR_MOE;
  }

  timer->CR1 |= TIM_CR1_CEN;

  mark_timer_initialized(timer);

  return true;
}

static bool pwm_channel_init(TIM_TypeDef* timer, uint8_t channel)
{
  if (timer == NULL)
  {
    return false;
  }

  if (!pwm_timer_base_init(timer))
  {
    return false;
  }

  switch (channel)
  {
    case 1U:
      timer->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_OC1PE);
      timer->CCMR1 |= (6U << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE;
      timer->CCR1 = 0U;
      timer->CCER |= TIM_CCER_CC1E;
      break;

    case 2U:
      timer->CCMR1 &= ~(TIM_CCMR1_OC2M | TIM_CCMR1_OC2PE);
      timer->CCMR1 |= (6U << TIM_CCMR1_OC2M_Pos) | TIM_CCMR1_OC2PE;
      timer->CCR2 = 0U;
      timer->CCER |= TIM_CCER_CC2E;
      break;

    case 3U:
      timer->CCMR2 &= ~(TIM_CCMR2_OC3M | TIM_CCMR2_OC3PE);
      timer->CCMR2 |= (6U << TIM_CCMR2_OC3M_Pos) | TIM_CCMR2_OC3PE;
      timer->CCR3 = 0U;
      timer->CCER |= TIM_CCER_CC3E;
      break;

    case 4U:
      timer->CCMR2 &= ~(TIM_CCMR2_OC4M | TIM_CCMR2_OC4PE);
      timer->CCMR2 |= (6U << TIM_CCMR2_OC4M_Pos) | TIM_CCMR2_OC4PE;
      timer->CCR4 = 0U;
      timer->CCER |= TIM_CCER_CC4E;
      break;

    default:
      return false;
  }

  timer->EGR = TIM_EGR_UG;

  return true;
}

static bool pwm_write_duty_per_mille(TIM_TypeDef* timer, uint8_t channel, uint16_t duty_per_mille)
{
  uint32_t period_counts = 0U;
  uint32_t compare_counts = 0U;

  if (timer == NULL)
  {
    return false;
  }

  if (duty_per_mille > 1000U)
  {
    duty_per_mille = 1000U;
  }

  period_counts = timer->ARR + 1U;
  compare_counts = (period_counts * (uint32_t)duty_per_mille) / 1000U;

  switch (channel)
  {
    case 1U:
      timer->CCR1 = compare_counts;
      break;

    case 2U:
      timer->CCR2 = compare_counts;
      break;

    case 3U:
      timer->CCR3 = compare_counts;
      break;

    case 4U:
      timer->CCR4 = compare_counts;
      break;

    default:
      return false;
  }

  return true;
}

/*
 * ==========================================================================
 *  CAN parameter helpers
 * ==========================================================================
 */

static uint8_t clamp_command_id(uint8_t command_id)
{
  return (uint8_t)(command_id & 0x0FU);
}

static char command_id_to_name_suffix(uint8_t command_id)
{
  command_id = clamp_command_id(command_id);

  if (command_id < 10U)
  {
    return (char)('0' + command_id);
  }

  return (char)('A' + (command_id - 10U));
}

static void build_velocity_param_name(size_t motor_index)
{
  char suffix = command_id_to_name_suffix(s_motors[motor_index].command_id);

  (void)snprintf(s_velocity_param_names[motor_index],
                 sizeof(s_velocity_param_names[motor_index]),
                 "%s.dc_motor_velocity_target_%c",
                 DC_MOTOR_SYSTEM_COMMAND_MESSAGE_NAME,
                 suffix);

  s_motors[motor_index].velocity_param_name = s_velocity_param_names[motor_index];
}

/*
 * ==========================================================================
 *  Motor control helpers
 * ==========================================================================
 */

static uint16_t velocity_to_duty_per_mille(int32_t velocity_deg_s)
{
  uint32_t abs_velocity = 0U;

  if (velocity_deg_s < 0)
  {
    abs_velocity = (uint32_t)(-velocity_deg_s);
  }
  else
  {
    abs_velocity = (uint32_t)velocity_deg_s;
  }

  if (abs_velocity == 0U)
  {
    return 0U;
  }

  if (abs_velocity >= DC_MOTOR_SYSTEM_MAX_VELOCITY_DEG_S)
  {
    return 1000U;
  }

  return (uint16_t)((abs_velocity * 1000U) / DC_MOTOR_SYSTEM_MAX_VELOCITY_DEG_S);
}

static void apply_motor_command(DcMotor_t* motor, int32_t velocity_deg_s)
{
  if (motor == NULL)
  {
    return;
  }

  motor->target_velocity_deg_s = velocity_deg_s;
  motor->duty_per_mille = velocity_to_duty_per_mille(velocity_deg_s);

  if (velocity_deg_s > 0)
  {
    HAL_GPIO_WritePin(motor->in_forward_port, motor->in_forward_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(motor->in_reverse_port, motor->in_reverse_pin, GPIO_PIN_RESET);
  }
  else if (velocity_deg_s < 0)
  {
    HAL_GPIO_WritePin(motor->in_forward_port, motor->in_forward_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(motor->in_reverse_port, motor->in_reverse_pin, GPIO_PIN_SET);
  }
  else
  {
    HAL_GPIO_WritePin(motor->in_forward_port, motor->in_forward_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(motor->in_reverse_port, motor->in_reverse_pin, GPIO_PIN_RESET);
  }

  (void)pwm_write_duty_per_mille(motor->pwm_timer,
                                 motor->pwm_channel,
                                 motor->duty_per_mille);
}

static void stop_all_motors(void)
{
  for (size_t i = 0U; i < DC_MOTOR_SYSTEM_NUM_MOTORS; i++)
  {
    apply_motor_command(&s_motors[i], 0);
  }
}

static void update_motor_commands_from_can(void)
{
  int32_t velocity_deg_s = 0;

  for (size_t i = 0U; i < DC_MOTOR_SYSTEM_NUM_MOTORS; i++)
  {
    if (CanParams_GetInt32(s_motors[i].velocity_param_name, &velocity_deg_s))
    {
      apply_motor_command(&s_motors[i], velocity_deg_s);
    }
  }
}

/*
 * ==========================================================================
 *  Public API
 * ==========================================================================
 */

bool dc_motor_system_init(void)
{
  for (size_t i = 0U; i < DC_MOTOR_SYSTEM_NUM_MOTORS; i++)
  {
    s_motors[i].command_id = clamp_command_id(s_motors[i].command_id);

    build_velocity_param_name(i);

    gpio_init_output(s_motors[i].in_forward_port,
                     s_motors[i].in_forward_pin);

    gpio_init_output(s_motors[i].in_reverse_port,
                     s_motors[i].in_reverse_pin);

    gpio_init_pwm_af(s_motors[i].enable_port,
                     s_motors[i].enable_pin,
                     s_motors[i].enable_af);

    if (!pwm_channel_init(s_motors[i].pwm_timer,
                          s_motors[i].pwm_channel))
    {
      return false;
    }
  }

  stop_all_motors();

  s_last_command_tick_ms = HAL_GetTick();
  s_initialized = true;

  return true;
}

void dc_motor_system_controller(void)
{
  uint32_t now_ms = 0U;

  if (!s_initialized)
  {
    if (!dc_motor_system_init())
    {
      return;
    }
  }

  now_ms = HAL_GetTick();

  if ((now_ms - s_last_command_tick_ms) >= DC_MOTOR_SYSTEM_COMMAND_PERIOD_MS)
  {
    s_last_command_tick_ms = now_ms;
    update_motor_commands_from_can();
  }
}
