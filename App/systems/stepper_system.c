#include "stepper_system.h"
#include "main.h"
#include "can_params.h"
#include "can_system.h"
#include <string.h>
#include <stdbool.h>

#define STEPPER_MOTOR_COUNT 6u

/* Multiplexor values for cmd signal */
#define MUX_CMD_POS_BASE  32  /* cmd=32-37 for position targets */
#define MUX_CMD_VEL_BASE  48  /* cmd=48-53 for velocity targets */
#define MUX_CMD_HB        16  /* cmd=16 for heartbeat */
#define MUX_CMD_LED       17  /* cmd=17 for LED */
#define MUX_CMD_MAINT_BASE 96 /* cmd=96-101 for maintenance commands */
#define MUX_CMD_SPEC_BASE 112 /* cmd=112-117 for spec requests */

/* State tracking for diffing */
static int32_t s_last_pos_tgt[STEPPER_MOTOR_COUNT];
static int32_t s_last_vel_tgt[STEPPER_MOTOR_COUNT];
static int32_t s_last_led = -1;
static int32_t s_last_hb  = -1;
static bool s_inited = false;

/* Signal names from DBC - message is STEPPER_PCB_C (ID 0x90) */
static const char* s_can_pos_tgt[STEPPER_MOTOR_COUNT] = {
    "STEPPER_PCB_C.motor_rel_pos_target_0", "STEPPER_PCB_C.motor_rel_pos_target_1",
    "STEPPER_PCB_C.motor_rel_pos_target_2", "STEPPER_PCB_C.motor_rel_pos_target_3",
    "STEPPER_PCB_C.motor_rel_pos_target_4", "STEPPER_PCB_C.motor_rel_pos_target_5"
};

static const char* s_can_vel_tgt[STEPPER_MOTOR_COUNT] = {
    "STEPPER_PCB_C.motor_velocity_target_0", "STEPPER_PCB_C.motor_velocity_target_1",
    "STEPPER_PCB_C.motor_velocity_target_2", "STEPPER_PCB_C.motor_velocity_target_3",
    "STEPPER_PCB_C.motor_velocity_target_4", "STEPPER_PCB_C.motor_velocity_target_5"
};

static const char* s_can_maint_cmd[STEPPER_MOTOR_COUNT] = {
    "STEPPER_PCB_C.maintenance_cmd_0", "STEPPER_PCB_C.maintenance_cmd_1",
    "STEPPER_PCB_C.maintenance_cmd_2", "STEPPER_PCB_C.maintenance_cmd_3",
    "STEPPER_PCB_C.maintenance_cmd_4", "STEPPER_PCB_C.maintenance_cmd_5"
};

static const char* s_can_spec_req[STEPPER_MOTOR_COUNT] = {
    "STEPPER_PCB_C.stepper_spec_req_event_0", "STEPPER_PCB_C.stepper_spec_req_event_1",
    "STEPPER_PCB_C.stepper_spec_req_event_2", "STEPPER_PCB_C.stepper_spec_req_event_3",
    "STEPPER_PCB_C.stepper_spec_req_event_4", "STEPPER_PCB_C.stepper_spec_req_event_5"
};

void stepper_system_controller(void) {
    // Initialization on first run
    if (!s_inited) {
        for (int i = 0; i < STEPPER_MOTOR_COUNT; i++) {
            s_last_pos_tgt[i] = -999999;
            s_last_vel_tgt[i] = -999999;
        }
        s_inited = true;
    }

    // Iterate through motors
    for (uint8_t i = 0; i < STEPPER_MOTOR_COUNT; i++) {
        int32_t val = 0;

        // Position Control (cmd = 32 + i)
        if (CanParams_GetInt32(s_can_pos_tgt[i], &val) && val != s_last_pos_tgt[i]) {
            s_last_pos_tgt[i] = val;
            /* Set the multiplexor first, then the signal value.
             * The can_system will pack both into the same frame.
             */
            CanSystem_SetInt32("STEPPER_PCB_C.cmd", MUX_CMD_POS_BASE + i);
            CanSystem_SetInt32(s_can_pos_tgt[i], val);
        }

        // Velocity Control (cmd = 48 + i)
        if (CanParams_GetInt32(s_can_vel_tgt[i], &val) && val != s_last_vel_tgt[i]) {
            s_last_vel_tgt[i] = val;
            CanSystem_SetInt32("STEPPER_PCB_C.cmd", MUX_CMD_VEL_BASE + i);
            CanSystem_SetInt32(s_can_vel_tgt[i], val);
        }
    }

    // LED Control (cmd = 17)
    int32_t led_val = 0;
    if (CanParams_GetInt32("STEPPER_PCB_C.led_status", &led_val) && led_val != s_last_led) {
        s_last_led = led_val;
        CanSystem_SetInt32("STEPPER_PCB_C.cmd", MUX_CMD_LED);
        CanSystem_SetInt32("STEPPER_PCB_C.led_status", led_val);
    }

    // Heartbeat Control (cmd = 16)
    int32_t hb_val = 0;
    if (CanParams_GetInt32("STEPPER_PCB_C.pcb_heartbeat_delay", &hb_val) && hb_val != s_last_hb) {
        s_last_hb = hb_val;
        CanSystem_SetInt32("STEPPER_PCB_C.cmd", MUX_CMD_HB);
        CanSystem_SetInt32("STEPPER_PCB_C.pcb_heartbeat_delay", hb_val);
    }
    
    // Maintenance Commands (cmd = 96-101)
    for (uint8_t i = 0; i < STEPPER_MOTOR_COUNT; i++) {
        int32_t maint_val = 0;
        if (CanParams_GetInt32(s_can_maint_cmd[i], &maint_val) && maint_val != 0) {
            // Only send if command is non-zero (0 = no action)
            CanSystem_SetInt32("STEPPER_PCB_C.cmd", MUX_CMD_MAINT_BASE + i);
            CanSystem_SetInt32(s_can_maint_cmd[i], maint_val);
            // Reset the command after sending to prevent re-sending
            CanParams_SetInt32(s_can_maint_cmd[i], 0);
        }
    }
    
    // Specification Requests (cmd = 112-117)
    for (uint8_t i = 0; i < STEPPER_MOTOR_COUNT; i++) {
        bool spec_req = false;
        if (CanParams_GetBool(s_can_spec_req[i], &spec_req) && spec_req) {
            CanSystem_SetInt32("STEPPER_PCB_C.cmd", MUX_CMD_SPEC_BASE + i);
            CanSystem_SetBool(s_can_spec_req[i], spec_req);
            // Reset the request after sending
            CanParams_SetBool(s_can_spec_req[i], false);
        }
    }
}


bool stepper_set_position(uint8_t motor_idx, int16_t rel_position) {
    if (motor_idx >= STEPPER_MOTOR_COUNT) return false;

    /* Set the multiplexor for this motor's position command */
    CanSystem_SetInt32("STEPPER_PCB_C.cmd", MUX_CMD_POS_BASE + motor_idx);
    return CanSystem_SetInt32(s_can_pos_tgt[motor_idx], rel_position);
}


bool stepper_set_position_target(uint8_t motor_idx, int16_t target_pos) {
    if (motor_idx >= STEPPER_MOTOR_COUNT) return false;
    
    int32_t current_pos = 0;
    if (stepper_get_position(motor_idx, &current_pos)) {
        int16_t rel_pos = target_pos - (int16_t)current_pos;
        return stepper_set_position(motor_idx, rel_pos);
    }
    return false;
}


bool stepper_set_velocity(uint8_t motor_idx, int16_t velocity) {
    if (motor_idx >= STEPPER_MOTOR_COUNT) return false;

    CanSystem_SetInt32("STEPPER_PCB_C.cmd", MUX_CMD_VEL_BASE + motor_idx);
    return CanSystem_SetInt32(s_can_vel_tgt[motor_idx], velocity);
}


bool stepper_set_led(bool on) {
    CanSystem_SetInt32("STEPPER_PCB_C.cmd", MUX_CMD_LED);
    return CanSystem_SetBool("STEPPER_PCB_C.led_status", on);
}


bool stepper_send_command(uint8_t motor_idx, uint8_t cmd) {
    if (motor_idx >= STEPPER_MOTOR_COUNT) return false;

    CanSystem_SetInt32("STEPPER_PCB_C.cmd", MUX_CMD_MAINT_BASE + motor_idx);
    return CanSystem_SetInt32(s_can_maint_cmd[motor_idx], cmd);
}


bool stepper_stop(uint8_t motor_idx) {
    return stepper_send_command(motor_idx, 1); // cmd = 1 for STOP
}

bool stepper_set_zero(uint8_t motor_idx) {
    return stepper_send_command(motor_idx, 0); // cmd = 0 for SET_ZERO
}


bool stepper_clear_error(uint8_t motor_idx) {
    return stepper_send_command(motor_idx, 3); // cmd = 3 for CLEAR_ERROR
}


bool stepper_shutdown(uint8_t motor_idx) {
    return stepper_send_command(motor_idx, 2); // cmd = 2 for SHUTDOWN
}


bool stepper_request_spec(uint8_t motor_idx) {
    if (motor_idx >= STEPPER_MOTOR_COUNT) return false;

    /* Set the multiplexor for spec request */
    CanSystem_SetInt32("STEPPER_PCB_C.cmd", MUX_CMD_SPEC_BASE + motor_idx);
    return CanSystem_SetBool(s_can_spec_req[motor_idx], true);
}


bool stepper_set_heartbeat(uint8_t delay_sec) {
    CanSystem_SetInt32("STEPPER_PCB_C.cmd", MUX_CMD_HB);
    return CanSystem_SetInt32("STEPPER_PCB_C.pcb_heartbeat_delay", delay_sec);
}

/*
 * Status values (per DBC):
 *   0: Undefined
 *   1: Idle
 *   2: Startup Sequence
 *   3: Error
 *   4: Error (Invalid Request)
 *   5: Error (Motor Disarmed)
 *   6: Error (Motor Failed)
 *   7: Error (Controller Failed)
 *   8: Error (ESTOP Requested)
 *   9: Error (Unknown Position)
 *   10: Position Control
 *   11: Velocity Control
 */
bool stepper_get_status(uint8_t motor_idx, int32_t* status) {
    if (motor_idx >= STEPPER_MOTOR_COUNT || status == NULL) return false;

    const char* status_names[6] = {
        "STEPPER_PCB_R.motor_status_0", "STEPPER_PCB_R.motor_status_1", "STEPPER_PCB_R.motor_status_2",
        "STEPPER_PCB_R.motor_status_3", "STEPPER_PCB_R.motor_status_4", "STEPPER_PCB_R.motor_status_5"
    };
    return CanParams_GetInt32(status_names[motor_idx], status);
}


bool stepper_get_position(uint8_t motor_idx, int32_t* position) {
    if (motor_idx >= STEPPER_MOTOR_COUNT || position == NULL) return false;

    const char* pos_names[6] = {
        "STEPPER_PCB_R.motor_position_0", "STEPPER_PCB_R.motor_position_1",
        "STEPPER_PCB_R.motor_position_2", "STEPPER_PCB_R.motor_position_3",
        "STEPPER_PCB_R.motor_position_4", "STEPPER_PCB_R.motor_position_5"
    };
    return CanParams_GetInt32(pos_names[motor_idx], position);
}


bool stepper_get_velocity(uint8_t motor_idx, int32_t* velocity) {
    if (motor_idx >= STEPPER_MOTOR_COUNT || velocity == NULL) return false;

    const char* vel_names[6] = {
        "STEPPER_PCB_R.motor_velocity_0", "STEPPER_PCB_R.motor_velocity_1",
        "STEPPER_PCB_R.motor_velocity_2", "STEPPER_PCB_R.motor_velocity_3",
        "STEPPER_PCB_R.motor_velocity_4", "STEPPER_PCB_R.motor_velocity_5"
    };
    return CanParams_GetInt32(vel_names[motor_idx], velocity);
}


bool stepper_has_inbox_pending(void) {
    bool pending = false;
    CanParams_GetBool("pending_inbox", &pending);
    return pending;
}


bool stepper_get_max_velocity(uint8_t motor_idx, uint16_t* max_vel) {
    if (motor_idx >= STEPPER_MOTOR_COUNT || max_vel == NULL) return false;

    const char* vel_names[6] = {
        "STEPPER_PCB_R.stepper_velocity_max_0", "STEPPER_PCB_R.stepper_velocity_max_1",
        "STEPPER_PCB_R.stepper_velocity_max_2", "STEPPER_PCB_R.stepper_velocity_max_3",
        "STEPPER_PCB_R.stepper_velocity_max_4", "STEPPER_PCB_R.stepper_velocity_max_5"
    };
    int32_t val = 0;
    if (CanParams_GetInt32(vel_names[motor_idx], &val)) {
        *max_vel = (uint16_t)val;
        return true;
    }
    return false;
}


bool stepper_get_step_angle(uint8_t motor_idx, float* step_angle) {
    if (motor_idx >= STEPPER_MOTOR_COUNT || step_angle == NULL) return false;

    const char* angle_names[6] = {
        "STEPPER_PCB_R.step_angle_0", "STEPPER_PCB_R.step_angle_1",
        "STEPPER_PCB_R.step_angle_2", "STEPPER_PCB_R.step_angle_3",
        "STEPPER_PCB_R.step_angle_4", "STEPPER_PCB_R.step_angle_5"
    };
    int32_t val = 0;
    if (CanParams_GetInt32(angle_names[motor_idx], &val)) {
        *step_angle = (float)val / 10.0f; // Convert from integer*10 to float
        return true;
    }
    return false;
}


bool stepper_get_maintenance_success(uint8_t motor_idx, uint8_t* success) {
    if (motor_idx >= STEPPER_MOTOR_COUNT || success == NULL) return false;

    const char* success_names[6] = {
        "STEPPER_PCB_R.maintenance_success_0", "STEPPER_PCB_R.maintenance_success_1",
        "STEPPER_PCB_R.maintenance_success_2", "STEPPER_PCB_R.maintenance_success_3",
        "STEPPER_PCB_R.maintenance_success_4", "STEPPER_PCB_R.maintenance_success_5"
    };
    int32_t val = 0;
    if (CanParams_GetInt32(success_names[motor_idx], &val)) {
        *success = (uint8_t)val;
        return true;
    }
    return false;
}

void stepper_system_test_spin_all(float speed) {
    for (uint8_t i = 0; i < STEPPER_MOTOR_COUNT; i++) {
        stepper_set_velocity(i, (int16_t)speed);
    }
}

