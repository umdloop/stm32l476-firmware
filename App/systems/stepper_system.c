#include "stepper_system.h"
#include "main.h"
#include "can_system.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

//PROTOCOL DEFINES

#define CAN_ID_STEPPER_REQUEST  0x90
#define CAN_ID_STEPPER_RESPONSE 0x91

#define CMD_BASE_POS_TARGET     0x20
#define CMD_BASE_VEL_TARGET     0x30
#define CMD_BASE_STATE          0x40
#define CMD_BASE_MAINTENANCE    0x60

#define MAINT_CMD_STOP          1
#define MAINT_CMD_SHUTDOWN      2

// 2. PINs

// Motor 0
#define step_0_Pin GPIO_PIN_3
#define step_0_GPIO_Port GPIOB
#define dir_0_Pin GPIO_PIN_2
#define dir_0_GPIO_Port GPIOD
#define enbl_0_Pin GPIO_PIN_4
#define enbl_0_GPIO_Port GPIOB

// Motor 1
#define step_1_Pin GPIO_PIN_5
#define step_1_GPIO_Port GPIOB
#define dir_1_Pin GPIO_PIN_6
#define dir_1_GPIO_Port GPIOB
#define enbl_1_Pin GPIO_PIN_13
#define enbl_1_GPIO_Port GPIOC

// Motor 2
#define step_2_Pin GPIO_PIN_8
#define step_2_GPIO_Port GPIOB
#define dir_2_Pin GPIO_PIN_14
#define dir_2_GPIO_Port GPIOC
#define enbl_2_Pin GPIO_PIN_3
#define enbl_2_GPIO_Port GPIOA

// Motor 3
#define step_3_Pin GPIO_PIN_5
#define step_3_GPIO_Port GPIOA
#define dir_3_Pin GPIO_PIN_2
#define dir_3_GPIO_Port GPIOA
#define enbl_3_Pin GPIO_PIN_4
#define enbl_3_GPIO_Port GPIOC

// Motor 4
#define step_4_Pin GPIO_PIN_0
#define step_4_GPIO_Port GPIOB
#define dir_4_Pin GPIO_PIN_7
#define dir_4_GPIO_Port GPIOC
#define enbl_4_Pin GPIO_PIN_8
#define enbl_4_GPIO_Port GPIOC

// Motor 5
#define step_5_Pin GPIO_PIN_10
#define step_5_GPIO_Port GPIOB
#define dir_5_Pin GPIO_PIN_2
#define dir_5_GPIO_Port GPIOB
#define enbl_5_Pin GPIO_PIN_14
#define enbl_5_GPIO_Port GPIOB

// 3. HARDWARE

typedef struct {
    GPIO_TypeDef* en_port;   uint16_t en_pin;
    GPIO_TypeDef* dir_port;  uint16_t dir_pin;
    TIM_HandleTypeDef* htim; uint32_t channel;
} StepperHw_t;

static StepperHw_t s_hw[STEPPER_PORT_COUNT] = {
    // Motor 0 (PB3/TIM2_CH2)
    { enbl_0_GPIO_Port, enbl_0_Pin, dir_0_GPIO_Port, dir_0_Pin, &htim2, TIM_CHANNEL_2 },
    // Motor 1 (PB5/TIM3_CH2)
    { enbl_1_GPIO_Port, enbl_1_Pin, dir_1_GPIO_Port, dir_1_Pin, &htim3, TIM_CHANNEL_2 },
    // Motor 2 (PB8/TIM4_CH3)
    { enbl_2_GPIO_Port, enbl_2_Pin, dir_2_GPIO_Port, dir_2_Pin, &htim4, TIM_CHANNEL_3 },
    // Motor 3 (PA5/TIM2_CH1)
    { enbl_3_GPIO_Port, enbl_3_Pin, dir_3_GPIO_Port, dir_3_Pin, &htim2, TIM_CHANNEL_1 },
    // Motor 4 (PB0/TIM3_CH3)
    { enbl_4_GPIO_Port, enbl_4_Pin, dir_4_GPIO_Port, dir_4_Pin, &htim3, TIM_CHANNEL_3 },
    // Motor 5 (PB10/TIM2_CH3)
    { enbl_5_GPIO_Port, enbl_5_Pin, dir_5_GPIO_Port, dir_5_Pin, &htim2, TIM_CHANNEL_3 },
};

typedef struct {
    float current_pos_deg;
    float target_vel_dps;
} PortState_t;

static PortState_t s_ports[STEPPER_PORT_COUNT] = {0};

// 4. LOGIC

static void set_motor_speed(uint8_t port, float dps) {
    if (port >= STEPPER_PORT_COUNT) return;

    s_ports[port].target_vel_dps = dps;
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
    int16_t pos = (int16_t)s_ports[port].current_pos_deg;
    data[1] = (uint8_t)(pos & 0xFF);
    data[2] = (uint8_t)((pos >> 8) & 0xFF);
    int16_t vel = (int16_t)s_ports[port].target_vel_dps;
    data[3] = (uint8_t)(vel & 0xFF);
    data[4] = (uint8_t)((vel >> 8) & 0xFF);
    CanSystem_Transmit(CAN_ID_STEPPER_RESPONSE, data, 5);
}

// 5. INITIALIZATION

static void stepper_gpio_init(void) {
    for (int i = 0; i < STEPPER_PORT_COUNT; i++) {
        GPIO_InitTypeDef GPIO_InitStruct = {0};

        // Enable Init
        GPIO_InitStruct.Pin = s_hw[i].en_pin;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(s_hw[i].en_port, &GPIO_InitStruct);

        // Direction Init
        GPIO_InitStruct.Pin = s_hw[i].dir_pin;
        HAL_GPIO_Init(s_hw[i].dir_port, &GPIO_InitStruct);

        // Set Defaults
        HAL_GPIO_WritePin(s_hw[i].en_port, s_hw[i].en_pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(s_hw[i].dir_port, s_hw[i].dir_pin, GPIO_PIN_RESET);
    }
}

static void stepper_timer_init(void) {
    for (int i = 0; i < STEPPER_PORT_COUNT; i++) {
        HAL_TIM_PWM_Start(s_hw[i].htim, s_hw[i].channel);
        __HAL_TIM_SET_COMPARE(s_hw[i].htim, s_hw[i].channel, 0);
    }
}

void stepper_system_init(void) {
    stepper_gpio_init();
    stepper_timer_init();
    memset(s_ports, 0, sizeof(s_ports));
}

// 6. CONTROLLER

void stepper_system_controller(void) {
    CanFrame_t frame;
    while (CanSystem_Receive(&frame)) {
        if (frame.can_id != CAN_ID_STEPPER_REQUEST) continue;

        uint8_t base_cmd = frame.data[0] & 0xF0;
        uint8_t motor_id = frame.data[0] & 0x0F;

        if (motor_id >= STEPPER_PORT_COUNT) continue;

        switch (base_cmd) {
            case CMD_BASE_VEL_TARGET: {
                int16_t raw_vel = (int16_t)(frame.data[1] | (frame.data[2] << 8));
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
