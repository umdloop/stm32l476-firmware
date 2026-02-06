#include "stepper_system.h"
#include "main.h"
#include "can_system.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

/* 1. DEFINES */
#define CAN_ID_STEPPER_REQUEST  0x90
#define CAN_ID_STEPPER_RESPONSE 0x91
#define CMD_BASE_VEL_TARGET   0x30
#define CMD_BASE_STATE        0x40
#define CMD_BASE_MAINTENANCE  0x60
#define MAINT_CMD_STOP        1
#define MAINT_CMD_SHUTDOWN    2

/* 2. TYPES & GLOBALS */
typedef struct {
  GPIO_TypeDef* en_port;   uint16_t en_pin;
  GPIO_TypeDef* dir_port;  uint16_t dir_pin;
  TIM_HandleTypeDef* htim; uint32_t channel;
} StepperHw_t;

static StepperHw_t s_hw[STEPPER_PORT_COUNT] = {
  { GPIOB, GPIO_PIN_4,  GPIOD, GPIO_PIN_2,  &htim2, TIM_CHANNEL_2 },
  { GPIOC, GPIO_PIN_13, GPIOB, GPIO_PIN_6,  &htim3, TIM_CHANNEL_2 },
  { GPIOA, GPIO_PIN_3,  GPIOC, GPIO_PIN_14, &htim4, TIM_CHANNEL_3 },
  { GPIOC, GPIO_PIN_4,  GPIOA, GPIO_PIN_2,  &htim2, TIM_CHANNEL_1 },
  { GPIOC, GPIO_PIN_8,  GPIOC, GPIO_PIN_7,  &htim3, TIM_CHANNEL_3 },
  { GPIOB, GPIO_PIN_14, GPIOB, GPIO_PIN_2,  &htim2, TIM_CHANNEL_3 },
};

static uint8_t s_inited = 0U;

/* 3. HELPER FUNCTIONS (Must be above controller) */

static void blink_led(uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
        HAL_Delay(100);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
        HAL_Delay(100);
    }
}

static void publish_state(uint8_t port, uint8_t base_cmd) {
  uint8_t data[5] = {0};
  data[0] = base_cmd + port;
  // (Simplified for test)
  CanSystem_Transmit(CAN_ID_STEPPER_RESPONSE, data, 5);
}

static void set_motor_speed(uint8_t port, float dps) {
  if (port >= STEPPER_PORT_COUNT) return;
  uint32_t freq = (uint32_t)abs((int)dps);
  HAL_GPIO_WritePin(s_hw[port].dir_port, s_hw[port].dir_pin, (dps >= 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);

  if (freq > 0) {
    uint32_t arr = 1000000 / freq;
    __HAL_TIM_SET_AUTORELOAD(s_hw[port].htim, arr);
    __HAL_TIM_SET_COMPARE(s_hw[port].htim, s_hw[port].channel, arr / 2);
  } else {
    __HAL_TIM_SET_COMPARE(s_hw[port].htim, s_hw[port].channel, 0);
  }
}

/* 4. MAIN CONTROLLER */
void stepper_system_controller(void) {
  if (!s_inited) {
    for (int i = 0; i < STEPPER_PORT_COUNT; i++) {
      HAL_TIM_PWM_Start(s_hw[i].htim, s_hw[i].channel);
      HAL_GPIO_WritePin(s_hw[i].en_port, s_hw[i].en_pin, GPIO_PIN_SET);
    }
    s_inited = 1U;
  }

  static uint32_t sequence_timer = 0;
  static uint8_t  test_step = 0;

  if (HAL_GetTick() - sequence_timer > 3000) {
    sequence_timer = HAL_GetTick();
    switch (test_step) {
      case 0:
        HAL_GPIO_WritePin(s_hw[0].en_port, s_hw[0].en_pin, GPIO_PIN_RESET);
        set_motor_speed(0, 1000.0f); blink_led(1); test_step = 1; break;
      case 1:
        set_motor_speed(0, 0.0f);
        HAL_GPIO_WritePin(s_hw[0].en_port, s_hw[0].en_pin, GPIO_PIN_SET);
        blink_led(2); test_step = 2; break;
      case 2:
        HAL_GPIO_WritePin(s_hw[0].en_port, s_hw[0].en_pin, GPIO_PIN_RESET);
        set_motor_speed(0, -500.0f); blink_led(3); test_step = 3; break;
      case 3:
        set_motor_speed(0, 0.0f);
        HAL_GPIO_WritePin(s_hw[0].en_port, s_hw[0].en_pin, GPIO_PIN_SET);
        blink_led(4); test_step = 0; break;
    }
  }

  CanFrame_t frame;
  while (CanSystem_Receive(&frame)) {
    if (frame.can_id != CAN_ID_STEPPER_REQUEST) continue;
    uint8_t motor_id = frame.data[0] & 0x07;
    uint8_t base_cmd = frame.data[0] & 0xF0;
    if (motor_id >= STEPPER_PORT_COUNT) continue;

    if (base_cmd == CMD_BASE_VEL_TARGET) {
      int16_t vel = (int16_t)(frame.data[1] | (frame.data[2] << 8));
      HAL_GPIO_WritePin(s_hw[motor_id].en_port, s_hw[motor_id].en_pin, GPIO_PIN_RESET);
      set_motor_speed(motor_id, (float)vel);
      publish_state(motor_id, base_cmd);
    } else if (base_cmd == CMD_BASE_MAINTENANCE) {
      if (frame.data[1] == MAINT_CMD_STOP) set_motor_speed(motor_id, 0);
      CanSystem_Transmit(CAN_ID_STEPPER_RESPONSE, frame.data, 1);
    }
  }
}
