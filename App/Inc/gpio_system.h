#ifndef GPIO_SYSTEM_H
#define GPIO_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/*
 * GPIO System configuration
 *
 * Analog reads use one ADC instance selected here. The default is ADC1.
 * Change GPIO_SYSTEM_ADC_INSTANCE if another ADC instance is preferred.
 */
#define GPIO_SYSTEM_ADC_INSTANCE ADC1

#define GPIO_SYSTEM_STATE_LOW   (0)
#define GPIO_SYSTEM_STATE_HIGH  (1)

/* Round-robin scheduled controller. Applies digital CAN assignments. */
void gpio_system_controller(void);

/* Configure pin as push-pull output and drive it HIGH(1) or LOW(0). */
bool GpioSystem_DigitalWrite(uint8_t pin_number, char pin_letter, uint8_t state);

/*
 * Configure pin as push-pull output and bind it to an existing CAN parameter.
 * The controller updates the pin each scheduler turn. The CAN parameter may be
 * Bool, Int32, or Float; zero is LOW, nonzero is HIGH.
 */
bool GpioSystem_DigitalAssign(uint8_t pin_number, char pin_letter, const char* can_parameter);

/*
 * Read a pin.
 * resolution == 1: configure as digital input and return 0 or 1.
 * resolution == 255 or 1024: configure as analog input and return scaled ADC value.
 * Returns -1 on failure.
 */
int GpioSystem_AnalogRead(uint8_t pin_number, char pin_letter, uint16_t resolution);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_SYSTEM_H */
