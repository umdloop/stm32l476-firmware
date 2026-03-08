#include "stepper_system.h"
#include "main.h"
#include "can_params.h"
#include "can_system.h"
#include <string.h>
#include <stdbool.h>

//CAN defs

#define STEPPER_CAN_REQ_ID  0x90
#define STEPPER_CAN_RES_ID  0x91
#define STEPPER_MOTOR_COUNT 8u

#define CMD_HEARTBEAT_DELAY 0x10
#define CMD_LED_STATUS      0x11
#define CMD_POS_RELATIVE    0x20
#define CMD_VELOCITY        0x30
#define CMD_STATE_REQUEST   0x40
#define CMD_STATUS_REQUEST  0x50
#define CMD_MAINTENANCE     0x60
#define CMD_SPECS_REQUEST   0x70

#define MAINT_SET_ZERO      0
#define MAINT_STOP          1
#define MAINT_SHUTDOWN      2
#define MAINT_CLEAR_ERRORS  3


static int32_t s_last_pos_tgt[STEPPER_MOTOR_COUNT];
static int32_t s_last_vel_tgt[STEPPER_MOTOR_COUNT];
static int32_t s_last_gen_cmd[STEPPER_MOTOR_COUNT];
static int32_t s_last_led = -1;
static int32_t s_last_hb  = -1;
static bool s_inited = false;


static const char* s_can_pos_tgt[STEPPER_MOTOR_COUNT] = {
    "STEPPER_PCB_C.motor_position_target_0", "STEPPER_PCB_C.motor_position_target_1",
    "STEPPER_PCB_C.motor_position_target_2", "STEPPER_PCB_C.motor_position_target_3",
    "STEPPER_PCB_C.motor_position_target_4", "STEPPER_PCB_C.motor_position_target_5",
    "STEPPER_PCB_C.motor_position_target_6", "STEPPER_PCB_C.motor_position_target_7"
};

static const char* s_can_vel_tgt[STEPPER_MOTOR_COUNT] = {
    "STEPPER_PCB_C.motor_velocity_target_0", "STEPPER_PCB_C.motor_velocity_target_1",
    "STEPPER_PCB_C.motor_velocity_target_2", "STEPPER_PCB_C.motor_velocity_target_3",
    "STEPPER_PCB_C.motor_velocity_target_4", "STEPPER_PCB_C.motor_velocity_target_5",
    "STEPPER_PCB_C.motor_velocity_target_6", "STEPPER_PCB_C.motor_velocity_target_7"
};

static const char* s_can_gen_cmd[STEPPER_MOTOR_COUNT] = {
    "STEPPER_PCB_C.general_0", "STEPPER_PCB_C.general_1", "STEPPER_PCB_C.general_2", "STEPPER_PCB_C.general_3",
    "STEPPER_PCB_C.general_4", "STEPPER_PCB_C.general_5", "STEPPER_PCB_C.general_6", "STEPPER_PCB_C.general_7"
};

static const char* s_can_pos_out[STEPPER_MOTOR_COUNT] = {
    "STEPPER_PCB_R.motor_position_0", "STEPPER_PCB_R.motor_position_1",
    "STEPPER_PCB_R.motor_position_2", "STEPPER_PCB_R.motor_position_3",
    "STEPPER_PCB_R.motor_position_4", "STEPPER_PCB_R.motor_position_5",
    "STEPPER_PCB_R.motor_position_6", "STEPPER_PCB_R.motor_position_7"
};

static const char* s_can_vel_out[STEPPER_MOTOR_COUNT] = {
    "STEPPER_PCB_R.motor_velocity_0", "STEPPER_PCB_R.motor_velocity_1",
    "STEPPER_PCB_R.motor_velocity_2", "STEPPER_PCB_R.motor_velocity_3",
    "STEPPER_PCB_R.motor_velocity_4", "STEPPER_PCB_R.motor_velocity_5",
    "STEPPER_PCB_R.motor_velocity_6", "STEPPER_PCB_R.motor_velocity_7"
};

static const char* s_can_status_out[STEPPER_MOTOR_COUNT] = {
    "STEPPER_PCB_R.motor_status_0", "STEPPER_PCB_R.motor_status_1",
    "STEPPER_PCB_R.motor_status_2", "STEPPER_PCB_R.motor_status_3",
    "STEPPER_PCB_R.motor_status_4", "STEPPER_PCB_R.motor_status_5",
    "STEPPER_PCB_R.motor_status_6", "STEPPER_PCB_R.motor_status_7"
};

static void send_request(uint8_t cmd_byte, uint8_t* data, uint8_t len) {
    uint8_t payload[8] = {0};
    payload[0] = cmd_byte;
    if (data && len > 0) {

    	uint8_t copy_len = (len > 7) ? 7 : len;
        memcpy(&payload[1], data, copy_len);
    }
    CanSystem_Transmit(STEPPER_CAN_REQ_ID, payload, (uint8_t)(len + 1));
}



void StepperSystem_ProcessResponse(uint8_t* payload, uint8_t len) {
    if (len < 1) return;

    uint8_t cmd_byte = payload[0];


    if (cmd_byte == CMD_HEARTBEAT_DELAY) {
        if (len >= 2) CanSystem_SetInt32("STEPPER_PCB_R.heartbeat_success", payload[1]);
        return;
    }
    if (cmd_byte == CMD_LED_STATUS) return;


    uint8_t motor_id = cmd_byte & 0x07;
    uint8_t type     = cmd_byte & 0xF0;

    if (motor_id >= STEPPER_MOTOR_COUNT) return;

    switch (type) {
        case CMD_POS_RELATIVE:
        case CMD_VELOCITY:
        case CMD_STATE_REQUEST:
            if (len >= 5) {
                int16_t pos = (int16_t)(payload[1] | (payload[2] << 8));
                int16_t vel = (int16_t)(payload[3] | (payload[4] << 8));
                CanSystem_SetInt32(s_can_pos_out[motor_id], (int32_t)pos);
                CanSystem_SetInt32(s_can_vel_out[motor_id], (int32_t)vel);
            }
            break;

        case CMD_STATUS_REQUEST:
            if (len >= 2) {
                CanSystem_SetInt32(s_can_status_out[motor_id], payload[1]);
            }
            break;

        case CMD_SPECS_REQUEST:
            if (len >= 4) {
                uint16_t max_v = (uint16_t)(payload[1] | (payload[2] << 8));
                uint8_t step_a = payload[3];
                CanSystem_SetInt32("STEPPER_PCB_R.max_velocity_spec", (int32_t)max_v);
                CanSystem_SetInt32("STEPPER_PCB_R.step_angle_spec", (int32_t)step_a);
            }
            break;

        default: break;
    }
}

void stepper_system_controller(void) {
    // Initialization on first run
    if (!s_inited) {
        for (int i = 0; i < STEPPER_MOTOR_COUNT; i++) {
            s_last_pos_tgt[i] = -999999;
            s_last_vel_tgt[i] = -999999;
            s_last_gen_cmd[i] = -1;
        }
        s_inited = true;
    }

    // Iterate through motors
    for (uint8_t i = 0; i < STEPPER_MOTOR_COUNT; i++) {
        int32_t val = 0;

        // Position Control
        if (CanParams_GetInt32(s_can_pos_tgt[i], &val)) {
            if (val != s_last_pos_tgt[i]) {
                s_last_pos_tgt[i] = val;
                uint8_t data[2] = { (uint8_t)(val & 0xFF), (uint8_t)((val >> 8) & 0xFF) };
                send_request(CMD_POS_RELATIVE + i, data, 2);
            }
        }

        // Velocity Control
        if (CanParams_GetInt32(s_can_vel_tgt[i], &val)) {
            if (val != s_last_vel_tgt[i]) {
                s_last_vel_tgt[i] = val;
                uint8_t data[2] = { (uint8_t)(val & 0xFF), (uint8_t)((val >> 8) & 0xFF) };
                send_request(CMD_VELOCITY + i, data, 2);
            }
        }

        // General Commands (Maintenance and Requests)
        if (CanParams_GetInt32(s_can_gen_cmd[i], &val)) {
            if (val != s_last_gen_cmd[i]) {
                s_last_gen_cmd[i] = val;
                switch (val) {
                    case 0: { uint8_t s = MAINT_SET_ZERO;     send_request(CMD_MAINTENANCE + i, &s, 1); break; }
                    case 1: { uint8_t s = MAINT_STOP;         send_request(CMD_MAINTENANCE + i, &s, 1); break; }
                    case 2: { uint8_t s = MAINT_SHUTDOWN;     send_request(CMD_MAINTENANCE + i, &s, 1); break; }
                    case 3: { uint8_t s = MAINT_CLEAR_ERRORS; send_request(CMD_MAINTENANCE + i, &s, 1); break; }
                    case 4: send_request(CMD_STATE_REQUEST + i, NULL, 0); break;
                    case 5: send_request(CMD_STATUS_REQUEST + i, NULL, 0); break;
                    case 6: send_request(CMD_SPECS_REQUEST + i, NULL, 0); break;
                }
            }
        }
    }

    // LED Control
    int32_t led_val = 0;
    if (CanParams_GetInt32("STEPPER_PCB_C.led_status", &led_val)) {
        if (led_val != s_last_led) {
            s_last_led = led_val;
            uint8_t data = (uint8_t)led_val;
            send_request(CMD_LED_STATUS, &data, 1);
        }
    }

    // Heartbeat Control
    int32_t hb_val = 0;
    if (CanParams_GetInt32("STEPPER_PCB_C.pcb_heartbeat_delay", &hb_val)) {
        if (hb_val != s_last_hb) {
            s_last_hb = hb_val;
            uint8_t data = (uint8_t)hb_val;
            send_request(CMD_HEARTBEAT_DELAY, &data, 1);
        }
    }
}
