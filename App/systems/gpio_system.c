#include "gpio_system.h"

#include "can_params.h"
#include "main.h"
#include "project_config.h"

#include <string.h>

/* =========================
 * GPIO System configuration
 * ========================= */

#define GPIO_SYSTEM_MAX_ASSIGNMENTS   (16U)
#define GPIO_SYSTEM_ADC_MAX_RAW       (4095U)

/*
 * Direct ADC timeout loop counts.
 * These are not milliseconds. They are simple loop guards to prevent lockups.
 */
#define GPIO_SYSTEM_ADC_TIMEOUT_LOOPS (1000000UL)

/*
 * ADC pin map for STM32L476RG.
 *
 * Currently this table is for ADC1.
 * If you change GPIO_SYSTEM_ADC_INSTANCE in gpio_system.h, update this table too.
 */
typedef struct
{
  char port_letter;
  uint8_t pin_number;
  uint32_t adc_channel;
} gpio_system_adc_pin_t;

static const gpio_system_adc_pin_t s_adc_pin_map[] =
{
  {'C', 0U, 1U},
  {'C', 1U, 2U},
  {'C', 2U, 3U},
  {'C', 3U, 4U},
  {'A', 0U, 5U},
  {'A', 1U, 6U},
  {'A', 2U, 7U},
  {'A', 3U, 8U},
  {'A', 4U, 9U},
  {'A', 5U, 10U},
  {'A', 6U, 11U},
  {'A', 7U, 12U},
  {'C', 4U, 13U},
  {'C', 5U, 14U},
  {'B', 0U, 15U},
  {'B', 1U, 16U},
};

/* =========================
 * Internal state
 * ========================= */

typedef struct
{
  GPIO_TypeDef* port;
  uint16_t pin_mask;
  char can_parameter[PROJECT_CAN_PARAM_NAME_MAX];
  uint8_t active;
} gpio_system_assignment_t;

static gpio_system_assignment_t s_assignments[GPIO_SYSTEM_MAX_ASSIGNMENTS];
static uint8_t s_adc_initialized = 0U;

/* =========================
 * Utilities
 * ========================= */

static char normalize_port_letter(char pin_letter)
{
  if (pin_letter >= 'a' && pin_letter <= 'z')
  {
    return (char)(pin_letter - ('a' - 'A'));
  }

  return pin_letter;
}

static bool get_port(char pin_letter, GPIO_TypeDef** out_port)
{
  if (out_port == NULL)
  {
    return false;
  }

  switch (normalize_port_letter(pin_letter))
  {
    case 'A':
      *out_port = GPIOA;
      return true;

    case 'B':
      *out_port = GPIOB;
      return true;

    case 'C':
      *out_port = GPIOC;
      return true;

#ifdef GPIOD
    case 'D':
      *out_port = GPIOD;
      return true;
#endif

#ifdef GPIOE
    case 'E':
      *out_port = GPIOE;
      return true;
#endif

#ifdef GPIOF
    case 'F':
      *out_port = GPIOF;
      return true;
#endif

#ifdef GPIOG
    case 'G':
      *out_port = GPIOG;
      return true;
#endif

#ifdef GPIOH
    case 'H':
      *out_port = GPIOH;
      return true;
#endif

    default:
      return false;
  }
}

static void enable_port_clock(char pin_letter)
{
  switch (normalize_port_letter(pin_letter))
  {
    case 'A':
      __HAL_RCC_GPIOA_CLK_ENABLE();
      break;

    case 'B':
      __HAL_RCC_GPIOB_CLK_ENABLE();
      break;

    case 'C':
      __HAL_RCC_GPIOC_CLK_ENABLE();
      break;

#ifdef GPIOD
    case 'D':
      __HAL_RCC_GPIOD_CLK_ENABLE();
      break;
#endif

#ifdef GPIOE
    case 'E':
      __HAL_RCC_GPIOE_CLK_ENABLE();
      break;
#endif

#ifdef GPIOF
    case 'F':
      __HAL_RCC_GPIOF_CLK_ENABLE();
      break;
#endif

#ifdef GPIOG
    case 'G':
      __HAL_RCC_GPIOG_CLK_ENABLE();
      break;
#endif

#ifdef GPIOH
    case 'H':
      __HAL_RCC_GPIOH_CLK_ENABLE();
      break;
#endif

    default:
      break;
  }
}

static bool get_pin(uint8_t pin_number, uint16_t* out_pin_mask)
{
  if (out_pin_mask == NULL || pin_number > 15U)
  {
    return false;
  }

  *out_pin_mask = (uint16_t)(1UL << pin_number);
  return true;
}

static bool configure_pin_output(GPIO_TypeDef* port, uint16_t pin_mask)
{
  if (port == NULL)
  {
    return false;
  }

  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = pin_mask;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;

  HAL_GPIO_Init(port, &gpio);
  return true;
}

static bool configure_pin_input(GPIO_TypeDef* port, uint16_t pin_mask)
{
  if (port == NULL)
  {
    return false;
  }

  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = pin_mask;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;

  HAL_GPIO_Init(port, &gpio);
  return true;
}

static bool configure_pin_analog(GPIO_TypeDef* port, uint16_t pin_mask)
{
  if (port == NULL)
  {
    return false;
  }

  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = pin_mask;
  gpio.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
  gpio.Pull = GPIO_NOPULL;

  HAL_GPIO_Init(port, &gpio);
  return true;
}

static bool lookup_adc_channel(uint8_t pin_number, char pin_letter, uint32_t* out_channel)
{
  if (out_channel == NULL)
  {
    return false;
  }

  char port = normalize_port_letter(pin_letter);

  for (uint32_t i = 0; i < (sizeof(s_adc_pin_map) / sizeof(s_adc_pin_map[0])); i++)
  {
    if (s_adc_pin_map[i].port_letter == port &&
        s_adc_pin_map[i].pin_number == pin_number)
    {
      *out_channel = s_adc_pin_map[i].adc_channel;
      return true;
    }
  }

  return false;
}

static bool read_can_parameter_as_bool(const char* can_parameter, bool* out_value)
{
  if (can_parameter == NULL || out_value == NULL)
  {
    return false;
  }

  bool b = false;
  if (CanParams_GetBool(can_parameter, &b))
  {
    *out_value = b;
    return true;
  }

  int32_t i32 = 0;
  if (CanParams_GetInt32(can_parameter, &i32))
  {
    *out_value = (i32 != 0);
    return true;
  }

  float f = 0.0f;
  if (CanParams_GetFloat(can_parameter, &f))
  {
    *out_value = (f != 0.0f);
    return true;
  }

  return false;
}

static bool wait_for_adc_flag(uint32_t flag)
{
  uint32_t timeout = GPIO_SYSTEM_ADC_TIMEOUT_LOOPS;

  while ((GPIO_SYSTEM_ADC_INSTANCE->ISR & flag) == 0U)
  {
    if (timeout-- == 0U)
    {
      return false;
    }
  }

  return true;
}

static bool wait_for_adc_clear(uint32_t flag)
{
  uint32_t timeout = GPIO_SYSTEM_ADC_TIMEOUT_LOOPS;

  while ((GPIO_SYSTEM_ADC_INSTANCE->CR & flag) != 0U)
  {
    if (timeout-- == 0U)
    {
      return false;
    }
  }

  return true;
}

static bool adc_init_once(void)
{
  if (s_adc_initialized)
  {
    return true;
  }

  __HAL_RCC_ADC_CLK_ENABLE();

  /*
   * Use synchronous ADC clock from PCLK.
   * This avoids needing a separate async ADC clock configuration.
   */
#ifdef ADC_CCR_CKMODE
  ADC123_COMMON->CCR &= ~ADC_CCR_CKMODE;
  ADC123_COMMON->CCR |= ADC_CCR_CKMODE_0;
#endif

  /*
   * Exit deep power-down and enable ADC voltage regulator.
   */
#ifdef ADC_CR_DEEPPWD
  GPIO_SYSTEM_ADC_INSTANCE->CR &= ~ADC_CR_DEEPPWD;
#endif

#ifdef ADC_CR_ADVREGEN
  GPIO_SYSTEM_ADC_INSTANCE->CR |= ADC_CR_ADVREGEN;
#endif

  for (volatile uint32_t i = 0; i < 10000U; i++)
  {
    __NOP();
  }

  /*
   * Make sure ADC is disabled before calibration.
   */
  if ((GPIO_SYSTEM_ADC_INSTANCE->CR & ADC_CR_ADEN) != 0U)
  {
    GPIO_SYSTEM_ADC_INSTANCE->CR |= ADC_CR_ADDIS;

    if (!wait_for_adc_clear(ADC_CR_ADEN))
    {
      return false;
    }
  }

  /*
   * Single-ended calibration.
   */
#ifdef ADC_CR_ADCALDIF
  GPIO_SYSTEM_ADC_INSTANCE->CR &= ~ADC_CR_ADCALDIF;
#endif

  GPIO_SYSTEM_ADC_INSTANCE->CR |= ADC_CR_ADCAL;

  if (!wait_for_adc_clear(ADC_CR_ADCAL))
  {
    return false;
  }

  /*
   * Enable ADC.
   */
  GPIO_SYSTEM_ADC_INSTANCE->ISR |= ADC_ISR_ADRDY;
  GPIO_SYSTEM_ADC_INSTANCE->CR |= ADC_CR_ADEN;

  if (!wait_for_adc_flag(ADC_ISR_ADRDY))
  {
    return false;
  }

  s_adc_initialized = 1U;
  return true;
}

static void adc_set_sample_time(uint32_t channel)
{
  /*
   * Use long sample time for easy testing with wires/pots/etc.
   * 7 means max sample time on STM32L4 ADC.
   */
  const uint32_t sample_time = 7U;

  if (channel <= 9U)
  {
    uint32_t shift = channel * 3U;
    GPIO_SYSTEM_ADC_INSTANCE->SMPR1 &= ~(7U << shift);
    GPIO_SYSTEM_ADC_INSTANCE->SMPR1 |=  (sample_time << shift);
  }
  else
  {
    uint32_t shift = (channel - 10U) * 3U;
    GPIO_SYSTEM_ADC_INSTANCE->SMPR2 &= ~(7U << shift);
    GPIO_SYSTEM_ADC_INSTANCE->SMPR2 |=  (sample_time << shift);
  }
}

static bool read_adc_raw(uint32_t adc_channel, uint16_t* out_raw)
{
  if (out_raw == NULL)
  {
    return false;
  }

  if (adc_channel < 1U || adc_channel > 16U)
  {
    return false;
  }

  if (!adc_init_once())
  {
    return false;
  }

  /*
   * Configure one regular conversion.
   */
  adc_set_sample_time(adc_channel);

  GPIO_SYSTEM_ADC_INSTANCE->SQR1 = 0U;
  GPIO_SYSTEM_ADC_INSTANCE->SQR1 |= (adc_channel << ADC_SQR1_SQ1_Pos);

  /*
   * Clear conversion flags.
   */
  GPIO_SYSTEM_ADC_INSTANCE->ISR |= ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR;

  /*
   * Start conversion.
   */
  GPIO_SYSTEM_ADC_INSTANCE->CR |= ADC_CR_ADSTART;

  if (!wait_for_adc_flag(ADC_ISR_EOC))
  {
    return false;
  }

  *out_raw = (uint16_t)(GPIO_SYSTEM_ADC_INSTANCE->DR & 0x0FFFU);

  return true;
}

static bool read_adc_scaled(uint32_t adc_channel, uint16_t resolution, int* out_value)
{
  if (out_value == NULL)
  {
    return false;
  }

  uint16_t raw = 0U;

  if (!read_adc_raw(adc_channel, &raw))
  {
    return false;
  }

  if (resolution == 255U)
  {
    *out_value = (int)(((uint32_t)raw * 255U + (GPIO_SYSTEM_ADC_MAX_RAW / 2U)) /
                       GPIO_SYSTEM_ADC_MAX_RAW);
    return true;
  }

  if (resolution == 1024U)
  {
    *out_value = (int)(((uint32_t)raw * 1024U + (GPIO_SYSTEM_ADC_MAX_RAW / 2U)) /
                       GPIO_SYSTEM_ADC_MAX_RAW);
    return true;
  }

  return false;
}

/* =========================
 * Public API
 * ========================= */

bool GpioSystem_DigitalWrite(uint8_t pin_number, char pin_letter, uint8_t state)
{
  if (state != GPIO_SYSTEM_STATE_LOW && state != GPIO_SYSTEM_STATE_HIGH)
  {
    return false;
  }

  GPIO_TypeDef* port = NULL;
  uint16_t pin_mask = 0U;

  if (!get_port(pin_letter, &port) || !get_pin(pin_number, &pin_mask))
  {
    return false;
  }

  enable_port_clock(pin_letter);

  if (!configure_pin_output(port, pin_mask))
  {
    return false;
  }

  HAL_GPIO_WritePin(port, pin_mask, state ? GPIO_PIN_SET : GPIO_PIN_RESET);

  return true;
}

bool GpioSystem_DigitalAssign(uint8_t pin_number, char pin_letter, const char* can_parameter)
{
  bool initial_value = false;

  if (!read_can_parameter_as_bool(can_parameter, &initial_value))
  {
    return false;
  }

  GPIO_TypeDef* port = NULL;
  uint16_t pin_mask = 0U;

  if (!get_port(pin_letter, &port) || !get_pin(pin_number, &pin_mask))
  {
    return false;
  }

  enable_port_clock(pin_letter);

  if (!configure_pin_output(port, pin_mask))
  {
    return false;
  }

  int free_index = -1;

  for (uint32_t i = 0; i < GPIO_SYSTEM_MAX_ASSIGNMENTS; i++)
  {
    if (s_assignments[i].active &&
        s_assignments[i].port == port &&
        s_assignments[i].pin_mask == pin_mask)
    {
      free_index = (int)i;
      break;
    }

    if (!s_assignments[i].active && free_index < 0)
    {
      free_index = (int)i;
    }
  }

  if (free_index < 0)
  {
    return false;
  }

  s_assignments[free_index].port = port;
  s_assignments[free_index].pin_mask = pin_mask;

  strncpy(s_assignments[free_index].can_parameter,
          can_parameter,
          PROJECT_CAN_PARAM_NAME_MAX - 1U);

  s_assignments[free_index].can_parameter[PROJECT_CAN_PARAM_NAME_MAX - 1U] = '\0';
  s_assignments[free_index].active = 1U;

  HAL_GPIO_WritePin(port,
                    pin_mask,
                    initial_value ? GPIO_PIN_SET : GPIO_PIN_RESET);

  return true;
}

int GpioSystem_AnalogRead(uint8_t pin_number, char pin_letter, uint16_t resolution)
{
  GPIO_TypeDef* port = NULL;
  uint16_t pin_mask = 0U;

  if (!get_port(pin_letter, &port) || !get_pin(pin_number, &pin_mask))
  {
    return -1;
  }

  enable_port_clock(pin_letter);

  if (resolution == 1U)
  {
    if (!configure_pin_input(port, pin_mask))
    {
      return -1;
    }

    return (HAL_GPIO_ReadPin(port, pin_mask) == GPIO_PIN_SET) ? 1 : 0;
  }

  if (resolution == 255U || resolution == 1024U)
  {
    uint32_t adc_channel = 0U;

    if (!lookup_adc_channel(pin_number, pin_letter, &adc_channel))
    {
      return -1;
    }

    if (!configure_pin_analog(port, pin_mask))
    {
      return -1;
    }

    int scaled = -1;

    if (!read_adc_scaled(adc_channel, resolution, &scaled))
    {
      return -1;
    }

    return scaled;
  }

  return -1;
}

void gpio_system_controller(void)
{
  for (uint32_t i = 0; i < GPIO_SYSTEM_MAX_ASSIGNMENTS; i++)
  {
    if (!s_assignments[i].active)
    {
      continue;
    }

    bool value = false;

    if (read_can_parameter_as_bool(s_assignments[i].can_parameter, &value))
    {
      HAL_GPIO_WritePin(s_assignments[i].port,
                        s_assignments[i].pin_mask,
                        value ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
  }
}
