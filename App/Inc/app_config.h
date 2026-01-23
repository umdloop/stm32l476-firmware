#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* System enable/disable flags */
#define SYSTEM_BLINK_ENABLED   (1)

/* Blink configuration */
#define BLINK_PERIOD_MS        (500U)

/* Onboard LED is PC5 */
#define LED_GPIO_PORT          GPIOC
#define LED_GPIO_PIN           GPIO_PIN_5

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H */
