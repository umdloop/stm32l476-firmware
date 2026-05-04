#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/*
 * Central project configuration
 * ----------------------------
 * Keep repo-wide tunables here so there is one place to review board settings,
 * CAN sizing, and demo behavior.
 */

/* =========================
 * Board GPIO configuration
 * =========================
 * Physical MCU pin used for the onboard status LED.
 */
#define PROJECT_LED_GPIO_PORT                 GPIOC
#define PROJECT_LED_GPIO_PIN                  GPIO_PIN_5

/* =========================
 * bxCAN timing configuration
 * =========================
 * These values are passed directly into HAL CAN init.
 * The default tuple below matches the original project timing setup.
 */
#define PROJECT_CAN_PRESCALER                 1U
#define PROJECT_CAN_SJW                       CAN_SJW_1TQ
#define PROJECT_CAN_BS1                       CAN_BS1_5TQ
#define PROJECT_CAN_BS2                       CAN_BS2_2TQ

/* =========================
 * DBC / CAN parser capacities
 * =========================
 * Increase only when the build requires it. Larger values consume more RAM.
 */
#define PROJECT_CAN_MAX_DBC_MSGS              40U
#define PROJECT_CAN_MAX_DBC_SIGS              220U
#define PROJECT_CAN_MAX_MUX_PAGES_PER_MSG     128U
#define PROJECT_CAN_MAX_PENDING_MUX_PAGES     8U

/* =========================
 * CAN parameter database sizes
 * =========================
 * Name hash table should stay a power-of-two.
 */
#define PROJECT_CAN_PARAM_MAX_PARAMS          160U
#define PROJECT_CAN_PARAM_NAME_MAX            64U
#define PROJECT_CAN_PARAM_HASH_SIZE           256U
#define PROJECT_CAN_PARAM_HASH_MAX_PROBE      32U

/* =========================
 * Hardware filter configuration
 * =========================
 * Set to 1 to install an explicit standard-ID allowlist into bxCAN filters.
 * Set to 0 to accept all IDs in hardware and rely only on software parsing.
 * 0 has predefined allowed hardware filters, you must assign them.
 */
#define PROJECT_CAN_USE_DEFAULT_RX_FILTER     1U

/* Default RX/TX arbitration IDs used by the shipped example systems.
 * Keep this list aligned with the DBC message families you expect the board
 * to consume or observe on the bus.
 */
#define PROJECT_CAN_ID_POWER_PCB_C            0x010U
#define PROJECT_CAN_ID_POWER_PCB_R            0x011U
#define PROJECT_CAN_ID_SCIENCE_DC_MOTOR_C     0x070U
#define PROJECT_CAN_ID_SCIENCE_DC_MOTOR_R     0x071U
#define PROJECT_CAN_ID_SCIENCE_SERVO_PCB_C    0x080U
#define PROJECT_CAN_ID_SCIENCE_SERVO_PCB_R    0x081U
#define PROJECT_CAN_ID_SCIENCE_STEPPER_PCB_C  0x090U
#define PROJECT_CAN_ID_SCIENCE_STEPPER_PCB_R  0x091U



/* =========================
 * Servo application sizing
 * =========================
 * Total physical ports on this board, and the subset actively controlled by
 * the current example servo application.
 */
#define PROJECT_SERVO_PORT_COUNT              8U
#define PROJECT_SERVO_ACTIVE_CAN_COUNT        6U

/* =========================
 * Example/demo controller timing
 * =========================
 * Periods used by optional example systems. Keeping them here makes it easy
 * to slow demos down when sniffing traffic on a live bus.
 */
#define PROJECT_EXAMPLE_STATUS_PERIOD_MS      1000U
#define PROJECT_EXAMPLE_SPEC_PERIOD_MS        2000U
#define PROJECT_EXAMPLE_API_PERIOD_MS         500U

/* =========================
 * Build-time DBC generation
 * =========================
 * Canonical DBC file that the checked-in build rule regenerates from.
 */
#define PROJECT_DBC_SOURCE_RELATIVE_PATH      "../dbc_latest_4.13.2026.dbc"

#ifdef __cplusplus
}
#endif

#endif /* PROJECT_CONFIG_H */
