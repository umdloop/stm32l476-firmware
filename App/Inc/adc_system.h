#ifndef ADC_SYSTEM_H
#define ADC_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

bool adc_system_init(void);

void adc_system_controller(void);

#ifdef __cplusplus
}
#endif

#endif
