#include "stepper_system.h"
#include "main.h"
#include "can_system.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

// New protocol

#define CAN_ID_STEPPER_REQUEST  0x90
#define CAN_ID_STEPPER_RESPONSE 0x91

#define CMD_BASE_POS_TARGET     0x20
#define CMD_BASE_VEL_TARGET     0x30
#define CMD_BASE_STATE          0x40
#define CMD_BASE_MAINTENANCE    0x60

#define MAINT_CMD_STOP          1
#define MAINT_CMD_SHUTDOWN      2

// Hardware pins n stuff
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

typedef struct {
  float current_pos_deg;
  float target_vel_dps;
} PortState_t;

static PortState_t s_ports[STEPPER_PORT_COUNT] = {0};
static uint8_t s_inited = 0U;

//logic

static void set_motor_speed(uint8_t port, float dps) {
    if (port >= STEPPER_PORT_COUNT) return;

    s_ports[port].target_vel_dps = dps; // Store state for CAN returns
    uint32_t freq = (uint32_t)abs((int)dps);

    // Set Direction
    HAL_GPIO_WritePin(s_hw[port].dir_port, s_hw[port].dir_pin,
                     (dps >= 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    if (freq > 0) {
        uint32_t arr = 1000000 / freq;
        if (arr < 2) arr = 2;
        __HAL_TIM_SET_AUTORELOAD(s_hw[port].htim, arr);
        __HAL_TIM_SET_COMPARE(s_hw[port].htim, s_hw[port].channel, arr / 2);
    } else {
        __HAL_TIM_SET_COMPARE(s_hw[port].htim, s_hw[port].channel, 0);
    }
}

static void publish_state(uint8_t port, uint8_t base_cmd) {
    uint8_t data[5];
    data[0] = base_cmd + port;

    // Position (16-bit, Little Endian)
    int16_t pos = (int16_t)s_ports[port].current_pos_deg;
    data[1] = (uint8_t)(pos & 0xFF);
    data[2] = (uint8_t)((pos >> 8) & 0xFF);

    // Velocity (16-bit, Little Endian)
    int16_t vel = (int16_t)s_ports[port].target_vel_dps;
    data[3] = (uint8_t)(vel & 0xFF);
    data[4] = (uint8_t)((vel >> 8) & 0xFF);

    CanSystem_Transmit(CAN_ID_STEPPER_RESPONSE, data, 5);
}

//controller

void stepper_system_controller(void) {
    if (!s_inited) {
        for (int i = 0; i < STEPPER_PORT_COUNT; i++) {
            HAL_TIM_PWM_Start(s_hw[i].htim, s_hw[i].channel);
            HAL_GPIO_WritePin(s_hw[i].en_port, s_hw[i].en_pin, GPIO_PIN_SET);
        }
        s_inited = 1U;
    }

    CanFrame_t frame;
    while (CanSystem_Receive(&frame)) {
        if (frame.can_id != CAN_ID_STEPPER_REQUEST) continue;

        uint8_t base_cmd = frame.data[0] & 0xF0;
        uint8_t motor_id = frame.data[0] & 0x0F;

        if (motor_id >= STEPPER_PORT_COUNT) continue;


        switch (base_cmd) {
            case CMD_BASE_VEL_TARGET: {
                int16_t raw_vel = (int16_t)(frame.data[1] | (frame.data[2] << 8)); //little Indian :()
                HAL_GPIO_WritePin(s_hw[motor_id].en_port, s_hw[motor_id].en_pin, GPIO_PIN_RESET);
                set_motor_speed(motor_id, (float)raw_vel);
                publish_state(motor_id, base_cmd);
                break;
            }

            case CMD_BASE_MAINTENANCE: {
                uint8_t sub_cmd = frame.data[1];
                if (sub_cmd == MAINT_CMD_STOP) {
                    set_motor_speed(motor_id, 0);
                }
                else if (sub_cmd == MAINT_CMD_SHUTDOWN) {
                    set_motor_speed(motor_id, 0);
                    HAL_GPIO_WritePin(s_hw[motor_id].en_port, s_hw[motor_id].en_pin, GPIO_PIN_SET);
                }
                CanSystem_Transmit(CAN_ID_STEPPER_RESPONSE, frame.data, 8);
                break;
            }

            case CMD_BASE_STATE:
                publish_state(motor_id, base_cmd);
                break;
        }
    }
}
