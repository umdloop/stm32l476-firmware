#include "gpio_system.h"

#include "can_params.h"
#include "main.h"
#include "project_config.h"

#include <string.h>

/* =========================
 * GPIO System configuration
 * ========================= */

#define GPIO_SYSTEM_MAX_ASSIGNMENTS   (16U)
#define GPIO_SYSTEM_ADC_TIMEOUT_MS    (10U)
#define GPIO_SYSTEM_ADC_MAX_RAW       (4095U)

/*
 * ADC pin map for STM32L476RG package using GPIO_SYSTEM_ADC_INSTANCE.
 * Edit this table if your board/package uses a different ADC pin mapping.
 */
typedef struct
{
  char port_letter;
  uint8_t pin_number;
  uint32_t adc_channel;
} gpio_system_adc_pin_t;

static const gpio_system_adc_pin_t s_adc_pin_map[] =
{
  {'C', 0U, ADC_CHANNEL_1},
  {'C', 1U, ADC_CHANNEL_2},
  {'C', 2U, ADC_CHANNEL_3},
  {'C', 3U, ADC_CHANNEL_4},
  {'A', 0U, ADC_CHANNEL_5},
  {'A', 1U, ADC_CHANNEL_6},
  {'A', 2U, ADC_CHANNEL_7},
  {'A', 3U, ADC_CHANNEL_8},
  {'A', 4U, ADC_CHANNEL_9},
  {'A', 5U, ADC_CHANNEL_10},
  {'A', 6U, ADC_CHANNEL_11},
  {'A', 7U, ADC_CHANNEL_12},
  {'C', 4U, ADC_CHANNEL_13},
  {'C', 5U, ADC_CHANNEL_14},
  {'B', 0U, ADC_CHANNEL_15},
  {'B', 1U, ADC_CHANNEL_16},
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
static ADC_HandleTypeDef s_hadc;
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
    return false;

  switch (normalize_port_letter(pin_letter))
  {
    case 'A': *out_port = GPIOA; return true;
    case 'B': *out_port = GPIOB; return true;
    case 'C': *out_port = GPIOC; return true;
#ifdef GPIOD
    case 'D': *out_port = GPIOD; return true;
#endif
#ifdef GPIOE
    case 'E': *out_port = GPIOE; return true;
#endif
#ifdef GPIOF
    case 'F': *out_port = GPIOF; return true;
#endif
#ifdef GPIOG
    case 'G': *out_port = GPIOG; return true;
#endif
#ifdef GPIOH
    case 'H': *out_port = GPIOH; return true;
#endif
    default: return false;
  }
}

static void enable_port_clock(char pin_letter)
{
  switch (normalize_port_letter(pin_letter))
  {
    case 'A': __HAL_RCC_GPIOA_CLK_ENABLE(); break;
    case 'B': __HAL_RCC_GPIOB_CLK_ENABLE(); break;
    case 'C': __HAL_RCC_GPIOC_CLK_ENABLE(); break;
#ifdef GPIOD
    case 'D': __HAL_RCC_GPIOD_CLK_ENABLE(); break;
#endif
#ifdef GPIOE
    case 'E': __HAL_RCC_GPIOE_CLK_ENABLE(); break;
#endif
#ifdef GPIOF
    case 'F': __HAL_RCC_GPIOF_CLK_ENABLE(); break;
#endif
#ifdef GPIOG
    case 'G': __HAL_RCC_GPIOG_CLK_ENABLE(); break;
#endif
#ifdef GPIOH
    case 'H': __HAL_RCC_GPIOH_CLK_ENABLE(); break;
#endif
    default: break;
  }
}

static bool get_pin(uint8_t pin_number, uint16_t* out_pin_mask)
{
  if (out_pin_mask == NULL || pin_number > 15U)
    return false;

  *out_pin_mask = (uint16_t)(1UL << pin_number);
  return true;
}

static bool configure_pin_output(GPIO_TypeDef* port, uint16_t pin_mask)
{
  if (port == NULL)
    return false;

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
    return false;

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
    return false;

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
    return false;

  char port = normalize_port_letter(pin_letter);
  for (size_t i = 0; i < (sizeof(s_adc_pin_map) / sizeof(s_adc_pin_map[0])); i++)
  {
    if (s_adc_pin_map[i].port_letter == port && s_adc_pin_map[i].pin_number == pin_number)
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
    return false;

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

static bool adc_init_once(void)
{
  if (s_adc_initialized)
    return true;

  __HAL_RCC_ADC_CLK_ENABLE();

  s_hadc.Instance = GPIO_SYSTEM_ADC_INSTANCE;
  s_hadc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  s_hadc.Init.Resolution = ADC_RESOLUTION_12B;
  s_hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  s_hadc.Init.ScanConvMode = ADC_SCAN_DISABLE;
  s_hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  s_hadc.Init.LowPowerAutoWait = DISABLE;
  s_hadc.Init.ContinuousConvMode = DISABLE;
  s_hadc.Init.NbrOfConversion = 1;
  s_hadc.Init.DiscontinuousConvMode = DISABLE;
  s_hadc.Init.NbrOfDiscConversion = 1;
  s_hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  s_hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  s_hadc.Init.DMAContinuousRequests = DISABLE;
  s_hadc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  s_hadc.Init.OversamplingMode = DISABLE;

  if (HAL_ADC_Init(&s_hadc) != HAL_OK)
    return false;

  (void)HAL_ADCEx_Calibration_Start(&s_hadc, ADC_SINGLE_ENDED);

  s_adc_initialized = 1U;
  return true;
}

static bool read_adc_scaled(uint32_t adc_channel, uint16_t resolution, int* out_value)
{
  if (out_value == NULL)
    return false;

  if (!adc_init_once())
    return false;

  ADC_ChannelConfTypeDef channel = {0};
  channel.Channel = adc_channel;
  channel.Rank = ADC_REGULAR_RANK_1;
  channel.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
  channel.SingleDiff = ADC_SINGLE_ENDED;
  channel.OffsetNumber = ADC_OFFSET_NONE;
  channel.Offset = 0;

  if (HAL_ADC_ConfigChannel(&s_hadc, &channel) != HAL_OK)
    return false;

  if (HAL_ADC_Start(&s_hadc) != HAL_OK)
    return false;

  if (HAL_ADC_PollForConversion(&s_hadc, GPIO_SYSTEM_ADC_TIMEOUT_MS) != HAL_OK)
  {
    (void)HAL_ADC_Stop(&s_hadc);
    return false;
  }

  uint32_t raw = HAL_ADC_GetValue(&s_hadc);
  (void)HAL_ADC_Stop(&s_hadc);

  if (resolution == 255U)
  {
    *out_value = (int)((raw * 255U + (GPIO_SYSTEM_ADC_MAX_RAW / 2U)) / GPIO_SYSTEM_ADC_MAX_RAW);
    return true;
  }

  if (resolution == 1024U)
  {
    *out_value = (int)((raw * 1024U + (GPIO_SYSTEM_ADC_MAX_RAW / 2U)) / GPIO_SYSTEM_ADC_MAX_RAW);
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
    return false;

  GPIO_TypeDef* port = NULL;
  uint16_t pin_mask = 0U;
  if (!get_port(pin_letter, &port) || !get_pin(pin_number, &pin_mask))
    return false;

  enable_port_clock(pin_letter);
  if (!configure_pin_output(port, pin_mask))
    return false;

  HAL_GPIO_WritePin(port, pin_mask, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
  return true;
}

bool GpioSystem_DigitalAssign(uint8_t pin_number, char pin_letter, const char* can_parameter)
{
  bool initial_value = false;
  if (!read_can_parameter_as_bool(can_parameter, &initial_value))
    return false;

  GPIO_TypeDef* port = NULL;
  uint16_t pin_mask = 0U;
  if (!get_port(pin_letter, &port) || !get_pin(pin_number, &pin_mask))
    return false;

  enable_port_clock(pin_letter);
  if (!configure_pin_output(port, pin_mask))
    return false;

  int free_index = -1;
  for (uint32_t i = 0; i < GPIO_SYSTEM_MAX_ASSIGNMENTS; i++)
  {
    if (s_assignments[i].active && s_assignments[i].port == port && s_assignments[i].pin_mask == pin_mask)
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
    return false;

  s_assignments[free_index].port = port;
  s_assignments[free_index].pin_mask = pin_mask;
  (void)strncpy(s_assignments[free_index].can_parameter, can_parameter, PROJECT_CAN_PARAM_NAME_MAX - 1U);
  s_assignments[free_index].can_parameter[PROJECT_CAN_PARAM_NAME_MAX - 1U] = '\0';
  s_assignments[free_index].active = 1U;

  HAL_GPIO_WritePin(port, pin_mask, initial_value ? GPIO_PIN_SET : GPIO_PIN_RESET);
  return true;
}

int GpioSystem_AnalogRead(uint8_t pin_number, char pin_letter, uint16_t resolution)
{
  GPIO_TypeDef* port = NULL;
  uint16_t pin_mask = 0U;
  if (!get_port(pin_letter, &port) || !get_pin(pin_number, &pin_mask))
    return -1;

  enable_port_clock(pin_letter);

  if (resolution == 1U)
  {
    if (!configure_pin_input(port, pin_mask))
      return -1;
    return (HAL_GPIO_ReadPin(port, pin_mask) == GPIO_PIN_SET) ? 1 : 0;
  }

  if (resolution == 255U || resolution == 1024U)
  {
    uint32_t adc_channel = 0U;
    if (!lookup_adc_channel(pin_number, pin_letter, &adc_channel))
      return -1;

    if (!configure_pin_analog(port, pin_mask))
      return -1;

    int scaled = -1;
    if (!read_adc_scaled(adc_channel, resolution, &scaled))
      return -1;
    return scaled;
  }

  return -1;
}

void gpio_system_controller(void)
{
  for (uint32_t i = 0; i < GPIO_SYSTEM_MAX_ASSIGNMENTS; i++)
  {
    if (!s_assignments[i].active)
      continue;

    bool value = false;
    if (read_can_parameter_as_bool(s_assignments[i].can_parameter, &value))
    {
      HAL_GPIO_WritePin(s_assignments[i].port,
                        s_assignments[i].pin_mask,
                        value ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
  }
}
