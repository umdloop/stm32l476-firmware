#ifndef STEPPER_SYSTEM_H
#define STEPPER_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>

#define STEPPER_PORT_COUNT 6

/* ============================================================================
 *  Main controller - call this periodically from your scheduler
 * ============================================================================ */
void stepper_system_controller(void);

/* ============================================================================
 *  Command API - Send commands to motors
 * ============================================================================ */

/**
 * @brief Set relative position target for a specific motor
 * @param motor_idx Motor index (0-5)
 * @param rel_position Relative position in degrees (-32768 to 32767)
 * @return true if command was queued for TX
 */
bool stepper_set_position(uint8_t motor_idx, int16_t rel_position);

/**
 * @brief Set absolute position target for a specific motor
 * @param motor_idx Motor index (0-5)
 * @param target_pos Target absolute position in degrees (-32768 to 32767)
 * @return true if command was queued for TX
 * @note This function calculates relative position from current position
 */
bool stepper_set_position_target(uint8_t motor_idx, int16_t target_pos);

/**
 * @brief Set velocity target for a specific motor
 * @param motor_idx Motor index (0-5)
 * @param velocity Velocity in degrees/second (-32768 to 32767)
 * @return true if command was queued for TX
 */
bool stepper_set_velocity(uint8_t motor_idx, int16_t velocity);

/**
 * @brief Send a general command to a specific motor
 * @param motor_idx Motor index (0-5)
 * @param cmd Command: 0=SET_ZERO, 1=REQ_VECTORS, 2=STOP, 3=CLEAR_ERROR
 * @return true if command was queued for TX
 */
bool stepper_send_command(uint8_t motor_idx, uint8_t cmd);

/**
 * @brief Stop a specific motor
 * @param motor_idx Motor index (0-5)
 * @return true if command was queued for TX
 */
bool stepper_stop(uint8_t motor_idx);

/**
 * @brief Set current position as zero for a specific motor
 * @param motor_idx Motor index (0-5)
 * @return true if command was queued for TX
 */
bool stepper_set_zero(uint8_t motor_idx);

/**
 * @brief Clear errors for a specific motor
 * @param motor_idx Motor index (0-5)
 * @return true if command was queued for TX
 */
bool stepper_clear_error(uint8_t motor_idx);

/**
 * @brief Shutdown a specific motor
 * @param motor_idx Motor index (0-5)
 * @return true if command was queued for TX
 */
bool stepper_shutdown(uint8_t motor_idx);

/**
 * @brief Request stepper specifications for a specific motor
 * @param motor_idx Motor index (0-5)
 * @return true if command was queued for TX
 */
bool stepper_request_spec(uint8_t motor_idx);

/**
 * @brief Control the onboard LED
 * @param on true to turn on, false to turn off
 * @return true if command was queued for TX
 */
bool stepper_set_led(bool on);

/**
 * @brief Set heartbeat interval
 * @param delay_sec Seconds between heartbeats (0-255, 0 = disabled)
 * @return true if command was queued for TX
 */
bool stepper_set_heartbeat(uint8_t delay_sec);

/* ============================================================================
 *  Response API - Read motor feedback
 * ============================================================================ */

/**
 * @brief Read motor status from response message
 * @param motor_idx Motor index (0-5)
 * @param status Output pointer for status value
 * @return true if value was read successfully
 */
bool stepper_get_status(uint8_t motor_idx, int32_t* status);

/**
 * @brief Read motor position from response message
 * @param motor_idx Motor index (0-5)
 * @param position Output pointer for position in degrees
 * @return true if value was read successfully
 */
bool stepper_get_position(uint8_t motor_idx, int32_t* position);

/**
 * @brief Read motor velocity from response message
 * @param motor_idx Motor index (0-5)
 * @param velocity Output pointer for velocity in degrees/second
 * @return true if value was read successfully
 */
bool stepper_get_velocity(uint8_t motor_idx, int32_t* velocity);

/**
 * @brief Check if new data has been received from the stepper PCB
 * @return true if inbox has pending data
 */
bool stepper_has_inbox_pending(void);

/* ============================================================================
 *  Stepper Specification API - Read motor specifications from response
 * ============================================================================ */

/**
 * @brief Read stepper maximum velocity from response message
 * @param motor_idx Motor index (0-5)
 * @param max_vel Output pointer for maximum velocity in degrees/second
 * @return true if value was read successfully
 */
bool stepper_get_max_velocity(uint8_t motor_idx, uint16_t* max_vel);

/**
 * @brief Read step angle from response message
 * @param motor_idx Motor index (0-5)
 * @param step_angle Output pointer for step angle in degrees (0-25.5)
 * @return true if value was read successfully
 */
bool stepper_get_step_angle(uint8_t motor_idx, float* step_angle);

/**
 * @brief Read maintenance success status from response message
 * @param motor_idx Motor index (0-5)
 * @param success Output pointer for success status (0=fail, 1=success)
 * @return true if value was read successfully
 */
bool stepper_get_maintenance_success(uint8_t motor_idx, uint8_t* success);

/* ============================================================================
 *  Test function
 * ============================================================================ */
void stepper_system_test_spin_all(float speed);

#endif
