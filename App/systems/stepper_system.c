#include "ex_system.h"
#include "app_config.h"
#include "can_params.h"
#include "can_system.h"
#include "main.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

/* =========================
 *  Stepper model definitions
 * ========================= */

typedef struct
{
  bool speed_control;
  bool position_control;
} StepperModes_t;

typedef struct
{
  const char* name;

  StepperModes_t modes;

  uint32_t steps_per_rev;
  uint32_t max_speed_steps_s;
  uint32_t max_accel_steps_s2;

  uint8_t microstep_mode;  // 1, 2, 4, 8, 16, 32

} StepperDef_t;

#define STEPPER_MODEL_NONE 0
#define STEPPER_MODEL_NEMA17 1

static const StepperDef_t s_stepper_defs[] =
{
  {
    .name = "NONE",
    .modes = { .speed_control = false, .position_control = false },
    .steps_per_rev = 0,
    .max_speed_steps_s = 0,
    .max_accel_steps_s2 = 0,
    .microstep_mode = 1,
  },
  {
    .name = "NEMA17",
    .modes = { .speed_control = true, .position_control = true },
    .steps_per_rev = 200,
    .max_speed_steps_s = 10000,  // Increased max to 10 kHz
    .max_accel_steps_s2 = 5000,
    .microstep_mode = 16,
  }
};

static const uint8_t s_stepper_defs_count = (uint8_t)(sizeof(s_stepper_defs) / sizeof(s_stepper_defs[0]));

/* =========================
 *  Hardware mapping
 * ========================= */

#define STEPPER_PORT_COUNT 6

typedef struct
{
  TIM_TypeDef*  tim;
  uint8_t       channel;  // 1, 2, 3, or 4
  uint8_t       timer_group; // Motors sharing the same timer

  GPIO_TypeDef* dir_port;
  uint16_t      dir_pin;

  GPIO_TypeDef* enbl_port;
  uint16_t      enbl_pin;

  GPIO_TypeDef* ms1_port;
  uint16_t      ms1_pin;

  GPIO_TypeDef* ms2_port;
  uint16_t      ms2_pin;

  GPIO_TypeDef* i1_port;
  uint16_t      i1_pin;

  GPIO_TypeDef* i2_port;
  uint16_t      i2_pin;

} StepperPortHw_t;

// Timer groups: 0=TIM2, 1=TIM3, 2=TIM4
static StepperPortHw_t s_hw[STEPPER_PORT_COUNT] =
{
  // Motor 0: step=PB3/TIM2_CH2, dir=PD2, enbl=PB4, ms1=PC12, ms2=PC11, i1=PC10, i2=N/A
  { TIM2, 2, 0, GPIOD, GPIO_PIN_2, GPIOB, GPIO_PIN_4, GPIOC, GPIO_PIN_12, GPIOC, GPIO_PIN_11, GPIOC, GPIO_PIN_10, NULL, 0 },

  // Motor 1: step=PB5/TIM3_CH2, dir=PB6, enbl=PC13, ms1=PC15, ms2=PH1, i1=PB9, i2=PB7
  { TIM3, 2, 1, GPIOB, GPIO_PIN_6, GPIOC, GPIO_PIN_13, GPIOC, GPIO_PIN_15, GPIOH, GPIO_PIN_1, GPIOB, GPIO_PIN_9, GPIOB, GPIO_PIN_7 },

  // Motor 2: step=PB8/TIM4_CH3, dir=PC14, enbl=PA3, ms1=PC2, ms2=PC3, i1=PC0, i2=PC1
  { TIM4, 3, 2, GPIOC, GPIO_PIN_14, GPIOA, GPIO_PIN_3, GPIOC, GPIO_PIN_2, GPIOC, GPIO_PIN_3, GPIOC, GPIO_PIN_0, GPIOC, GPIO_PIN_1 },

  // Motor 3: step=PA5/TIM2_CH1, dir=PA2, enbl=PC4, ms1=PA6, ms2=PC5, i1=PA7, i2=PA4
  { TIM2, 1, 0, GPIOA, GPIO_PIN_2, GPIOC, GPIO_PIN_4, GPIOA, GPIO_PIN_6, GPIOC, GPIO_PIN_5, GPIOA, GPIO_PIN_7, GPIOA, GPIO_PIN_4 },

  // Motor 4: step=PB0/TIM3_CH3, dir=PC7, enbl=PC8, ms1=PA8, ms2=PC9, i1=PA10, i2=PA9
  { TIM3, 3, 1, GPIOC, GPIO_PIN_7, GPIOC, GPIO_PIN_8, GPIOA, GPIO_PIN_8, GPIOC, GPIO_PIN_9, GPIOA, GPIO_PIN_10, GPIOA, GPIO_PIN_9 },

  // Motor 5: step=PB10/TIM2_CH3, dir=PB2, enbl=PB14, ms1=PC6, ms2=PB12, i1=PB15, i2=PB11
  { TIM2, 3, 0, GPIOB, GPIO_PIN_2, GPIOB, GPIO_PIN_14, GPIOC, GPIO_PIN_6, GPIOB, GPIO_PIN_12, GPIOB, GPIO_PIN_15, GPIOB, GPIO_PIN_11 },
};

/* =========================
 *  State
 * ========================= */

typedef struct
{
  uint8_t  model_id;
  int32_t  target_position_steps;
  int32_t  current_position_steps;
  int32_t  target_speed_steps_s;
  bool     is_enabled;
  bool     is_moving;
} StepperPortState_t;

static StepperPortState_t s_ports[STEPPER_PORT_COUNT];
static uint8_t s_inited = 0U;

// Track current frequency for each timer group
#define TIMER_GROUP_COUNT 3
static uint32_t s_timer_group_freq[TIMER_GROUP_COUNT] = {0, 0, 0};

/* =========================
 *  CAN integration
 * ========================= */

static const char* s_can_general[STEPPER_PORT_COUNT] =
{
  "STEPPER_PCB_C.general_0",
  "STEPPER_PCB_C.general_1",
  "STEPPER_PCB_C.general_2",
  "STEPPER_PCB_C.general_3",
  "STEPPER_PCB_C.general_4",
  "STEPPER_PCB_C.general_5",
};

static const char* s_can_speed_cmd[STEPPER_PORT_COUNT] =
{
  "STEPPER_PCB_C.motor_speed_cmd_0",
  "STEPPER_PCB_C.motor_speed_cmd_1",
  "STEPPER_PCB_C.motor_speed_cmd_2",
  "STEPPER_PCB_C.motor_speed_cmd_3",
  "STEPPER_PCB_C.motor_speed_cmd_4",
  "STEPPER_PCB_C.motor_speed_cmd_5",
};

static const char* s_can_status_out[STEPPER_PORT_COUNT] =
{
  "STEPPER_PCB_R.motor_status_0",
  "STEPPER_PCB_R.motor_status_1",
  "STEPPER_PCB_R.motor_status_2",
  "STEPPER_PCB_R.motor_status_3",
  "STEPPER_PCB_R.motor_status_4",
  "STEPPER_PCB_R.motor_status_5",
};

static const char* s_can_heartbeat = "STEPPER_PCB_C.heartbeat_mode";

static uint8_t s_rx_inited = 0U;
static int32_t s_last_general[STEPPER_PORT_COUNT];
static int32_t s_last_speed_cmd[STEPPER_PORT_COUNT];

/* Weak callbacks */
__attribute__((weak)) void StepperSystem_OnMotorStop(uint8_t port)     { (void)port; }
__attribute__((weak)) void StepperSystem_OnMotorShutdown(uint8_t port) { (void)port; }
__attribute__((weak)) void StepperSystem_OnStatusRequest(uint8_t port) { (void)port; }

/* =========================
 *  Helpers
 * ========================= */

static bool is_port_valid(uint8_t port) { return (port < STEPPER_PORT_COUNT); }

static const StepperDef_t* get_def(uint8_t model_id)
{
  if (model_id >= s_stepper_defs_count) return NULL;
  return &s_stepper_defs[model_id];
}

static int32_t clamp_i32(int32_t x, int32_t lo, int32_t hi)
{
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

/* ACTIVE-LOW enable (pulled high = OFF) */
static void set_enable(uint8_t port, bool enable)
{
  HAL_GPIO_WritePin(s_hw[port].enbl_port, s_hw[port].enbl_pin,
                    enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
  s_ports[port].is_enabled = enable;
}

static void set_direction(uint8_t port, bool forward)
{
  HAL_GPIO_WritePin(s_hw[port].dir_port, s_hw[port].dir_pin,
                    forward ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void set_microstep_mode(uint8_t port, uint8_t mode)
{
  // MS1, MS2 configuration for different microstep modes
  // mode: 1=full, 2=half, 4=1/4, 8=1/8, 16=1/16, 32=1/32
  bool ms1 = false, ms2 = false;
  
  switch(mode)
  {
    case 1:  ms1 = false; ms2 = false; break; // Full step
    case 2:  ms1 = true;  ms2 = false; break; // Half step
    case 4:  ms1 = false; ms2 = true;  break; // 1/4 step
    case 8:  ms1 = true;  ms2 = true;  break; // 1/8 step
    case 16: ms1 = true;  ms2 = true;  break; // 1/16 step (A4988 specific)
    default: ms1 = false; ms2 = false; break;
  }
  
  HAL_GPIO_WritePin(s_hw[port].ms1_port, s_hw[port].ms1_pin,
                    ms1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(s_hw[port].ms2_port, s_hw[port].ms2_pin,
                    ms2 ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void tim_set_ccr(TIM_TypeDef* tim, uint8_t ch, uint32_t value)
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

static void set_step_frequency(uint8_t port, uint32_t freq_hz)
{
  // Timer configuration from IOC:
  // - Timer clock: 8 MHz
  // - Prescaler: 7 → Effective clock = 8MHz / (7+1) = 1 MHz
  // - Default ARR: 999

  const uint32_t timer_clock = 1000000; // 1 MHz after prescaler
  uint8_t timer_group = s_hw[port].timer_group;
  
  if (freq_hz == 0)
  {
    // Stop this channel
    tim_set_ccr(s_hw[port].tim, s_hw[port].channel, 0);
    s_ports[port].is_moving = false;

    // Check if any other motors in this timer group are still moving
    bool any_moving = false;
    for (uint8_t i = 0; i < STEPPER_PORT_COUNT; i++)
    {
      if (s_hw[i].timer_group == timer_group && i != port && s_ports[i].is_moving)
      {
        any_moving = true;
        break;
      }
    }

    // If no motors in this group are moving, we could stop the timer
    // but it's safer to just leave it running with 0 duty cycle

    return;
  }

  // Clamp frequency to safe range
  if (freq_hz < 10) freq_hz = 10;       // Minimum 10 Hz
  if (freq_hz > 10000) freq_hz = 10000; // Maximum 10 kHz

  // Calculate new ARR for this frequency
  uint32_t arr = (timer_clock / freq_hz) - 1;

  // Update the timer period (affects all channels on this timer!)
  s_hw[port].tim->ARR = arr;
  s_timer_group_freq[timer_group] = freq_hz;

  // Set 50% duty cycle for this channel
  tim_set_ccr(s_hw[port].tim, s_hw[port].channel, arr / 2);

  // Update duty cycles for other active channels on this timer
  for (uint8_t i = 0; i < STEPPER_PORT_COUNT; i++)
  {
    if (s_hw[i].timer_group == timer_group && i != port && s_ports[i].is_moving)
    {
      tim_set_ccr(s_hw[i].tim, s_hw[i].channel, arr / 2);
    }
  }

  s_ports[port].is_moving = true;
}

/* =========================
 *  Init once
 * ========================= */

static void init_internal_once(void)
{
  memset(s_ports, 0, sizeof(s_ports));
  for (uint8_t i = 0; i < STEPPER_PORT_COUNT; i++)
  {
    s_ports[i].model_id = STEPPER_MODEL_NONE;
    s_ports[i].current_position_steps = 0;
    s_ports[i].target_position_steps = 0;
    s_ports[i].target_speed_steps_s = 0;
    s_ports[i].is_enabled = false;
    s_ports[i].is_moving = false;
  }

  s_rx_inited = 0U;
  for (uint8_t i = 0; i < STEPPER_PORT_COUNT; i++)
  {
    s_last_general[i] = -999999;
    s_last_speed_cmd[i] = -999999;
  }

  // Initialize timer group frequencies
  for (uint8_t i = 0; i < TIMER_GROUP_COUNT; i++)
  {
    s_timer_group_freq[i] = 0;
  }

  // GPIO clocks should already be enabled by HAL/CubeMX
  // Initialize all control pins to safe state
  
  for (uint8_t i = 0; i < STEPPER_PORT_COUNT; i++)
  {
    // Set direction to forward (default)
    HAL_GPIO_WritePin(s_hw[i].dir_port, s_hw[i].dir_pin, GPIO_PIN_RESET);

    // Disable all motors (active-low enable, so set high)
    HAL_GPIO_WritePin(s_hw[i].enbl_port, s_hw[i].enbl_pin, GPIO_PIN_SET);

    // Set microstep pins to full step mode
    HAL_GPIO_WritePin(s_hw[i].ms1_port, s_hw[i].ms1_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(s_hw[i].ms2_port, s_hw[i].ms2_pin, GPIO_PIN_RESET);

    // Set current sense outputs to low
    HAL_GPIO_WritePin(s_hw[i].i1_port, s_hw[i].i1_pin, GPIO_PIN_RESET);
    if (s_hw[i].i2_port != NULL)
    {
      HAL_GPIO_WritePin(s_hw[i].i2_port, s_hw[i].i2_pin, GPIO_PIN_RESET);
    }

    // Stop PWM output (0% duty cycle)
    tim_set_ccr(s_hw[i].tim, s_hw[i].channel, 0);
  }

  // Start PWM timers - they should be configured by CubeMX
  // Start each timer only once per timer group
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1); // Motor 3
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2); // Motor 0
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3); // Motor 5

  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2); // Motor 1
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3); // Motor 4

  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3); // Motor 2

  // Default first 6 ports to NEMA17 model (disabled)
  for (uint8_t p = 0; p < STEPPER_PORT_COUNT; p++)
  {
    s_ports[p].model_id = STEPPER_MODEL_NEMA17;
    set_enable(p, false);
    set_microstep_mode(p, s_stepper_defs[STEPPER_MODEL_NEMA17].microstep_mode);
  }
}

/* =========================
 *  Actions
 * ========================= */

static void stop_motor(uint8_t port)
{
  if (!is_port_valid(port)) return;
  
  set_step_frequency(port, 0);
  s_ports[port].target_speed_steps_s = 0;
  
  // Publish stop confirmation
  (void)CanSystem_SetBool("STEPPER_PCB_R.motor_stop_confirm", true);
}

static void shutdown_motor(uint8_t port)
{
  if (!is_port_valid(port)) return;
  
  stop_motor(port);
  set_enable(port, false);
  
  // Publish shutdown confirmation
  (void)CanSystem_SetBool("STEPPER_PCB_R.motor_shutdown_confirm", true);
}

static void publish_status(uint8_t port)
{
  if (!is_port_valid(port)) return;
  
  // Publish motor status (format as needed)
  // Could be: position | speed | enabled | moving
  int32_t status = (s_ports[port].current_position_steps & 0xFFFF) |
                   ((s_ports[port].is_enabled ? 1 : 0) << 16) |
                   ((s_ports[port].is_moving ? 1 : 0) << 17);
  
  (void)CanSystem_SetInt32(s_can_status_out[port], status);
}

/* =========================
 *  Public API
 * ========================= */

bool StepperSystem_SetStepperModel(uint8_t port, uint8_t model_id)
{
  if (!s_inited)
  {
    s_inited = 1U;
    init_internal_once();
  }

  if (!is_port_valid(port)) return false;

  const StepperDef_t* def = get_def(model_id);
  if (def == NULL) return false;

  s_ports[port].model_id = model_id;

  if (model_id == STEPPER_MODEL_NONE)
  {
    set_enable(port, false);
    set_step_frequency(port, 0);
    return true;
  }

  set_enable(port, false); // Start disabled
  set_microstep_mode(port, def->microstep_mode);
  set_step_frequency(port, 0);

  return true;
}

uint8_t StepperSystem_GetStepperModel(uint8_t port)
{
  if (!is_port_valid(port)) return STEPPER_MODEL_NONE;
  return s_ports[port].model_id;
}

bool StepperSystem_SetSpeedStepsS(uint8_t port, int32_t speed_steps_s)
{
  if (!s_inited)
  {
    s_inited = 1U;
    init_internal_once();
  }

  if (!is_port_valid(port)) return false;

  const StepperDef_t* def = get_def(s_ports[port].model_id);
  if ((def == NULL) || !def->modes.speed_control) return false;

  // Clamp speed to max
  int32_t max = (int32_t)def->max_speed_steps_s;
  speed_steps_s = clamp_i32(speed_steps_s, -max, max);

  s_ports[port].target_speed_steps_s = speed_steps_s;

  // Set direction based on sign
  set_direction(port, speed_steps_s >= 0);

  // Enable motor and set frequency
  set_enable(port, true);
  set_step_frequency(port, (uint32_t)abs(speed_steps_s));

  return true;
}

void StepperSystem_Controller(void)
{
  if (!s_inited)
  {
    s_inited = 1U;
    init_internal_once();
  }

  // Handle heartbeat mode (if needed for keepalive)
  bool heartbeat = false;
  if (CanParams_GetBool(s_can_heartbeat, &heartbeat))
  {
    // Could toggle an LED or send periodic status
  }

  // Process each stepper motor
  for (uint8_t i = 0; i < STEPPER_PORT_COUNT; i++)
  {
    // Handle general commands
    int32_t gen = 0;
    if (CanParams_GetInt32(s_can_general[i], &gen))
    {
      if (!s_rx_inited || (gen != s_last_general[i]))
      {
        s_last_general[i] = gen;

        switch ((uint8_t)gen)
        {
          case 0: // Reserved or no-op
            break;
          case 1: // Request status
            StepperSystem_OnStatusRequest(i);
            publish_status(i);
            break;
          case 2: // Motor stop
            StepperSystem_OnMotorStop(i);
            stop_motor(i);
            break;
          case 3: // Motor shutdown
            StepperSystem_OnMotorShutdown(i);
            shutdown_motor(i);
            break;
          default:
            break;
        }
      }
    }

    // Handle speed command
    int32_t speed_cmd = 0;
    if (CanParams_GetInt32(s_can_speed_cmd[i], &speed_cmd))
    {
      if (!s_rx_inited || (speed_cmd != s_last_speed_cmd[i]))
      {
        s_last_speed_cmd[i] = speed_cmd;
        (void)StepperSystem_SetSpeedStepsS(i, speed_cmd);
      }
    }
  }

  s_rx_inited = 1U;
}

void stepper_system_controller(void)
{
  StepperSystem_Controller();
}
