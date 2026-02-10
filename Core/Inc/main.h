

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l4xx_hal.h"



/* USER CODE BEGIN EFP */

/* Central error handler used across Platform/App */
void Error_Handler(void);

/* USER CODE END EFP */

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
