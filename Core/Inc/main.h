

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l4xx_hal.h"

#define enbl_1_Pin GPIO_PIN_13
#define enbl_1_GPIO_Port GPIOC
#define dir_2_Pin GPIO_PIN_14
#define dir_2_GPIO_Port GPIOC
#define ms1_1_Pin GPIO_PIN_15
#define ms1_1_GPIO_Port GPIOC
#define ms2_1_Pin GPIO_PIN_1
#define ms2_1_GPIO_Port GPIOH
#define i1_2_Pin GPIO_PIN_0
#define i1_2_GPIO_Port GPIOC
#define i2_2_Pin GPIO_PIN_1
#define i2_2_GPIO_Port GPIOC
#define ms1_2_Pin GPIO_PIN_2
#define ms1_2_GPIO_Port GPIOC
#define ms2_2_Pin GPIO_PIN_3
#define ms2_2_GPIO_Port GPIOC
#define dir_3_Pin GPIO_PIN_2
#define dir_3_GPIO_Port GPIOA
#define enbl_2_Pin GPIO_PIN_3
#define enbl_2_GPIO_Port GPIOA
#define i2_3_Pin GPIO_PIN_4
#define i2_3_GPIO_Port GPIOA
#define step_3_Pin GPIO_PIN_5
#define step_3_GPIO_Port GPIOA
#define ms1_3_Pin GPIO_PIN_6
#define ms1_3_GPIO_Port GPIOA
#define i1_3_Pin GPIO_PIN_7
#define i1_3_GPIO_Port GPIOA
#define enbl_3_Pin GPIO_PIN_4
#define enbl_3_GPIO_Port GPIOC
#define ms2_3_Pin GPIO_PIN_5
#define ms2_3_GPIO_Port GPIOC
#define step_4_Pin GPIO_PIN_0
#define step_4_GPIO_Port GPIOB
#define dir_5_Pin GPIO_PIN_2
#define dir_5_GPIO_Port GPIOB
#define step_5_Pin GPIO_PIN_10
#define step_5_GPIO_Port GPIOB
#define i2_5_Pin GPIO_PIN_11
#define i2_5_GPIO_Port GPIOB
#define ms2_5_Pin GPIO_PIN_12
#define ms2_5_GPIO_Port GPIOB
#define enbl_5_Pin GPIO_PIN_14
#define enbl_5_GPIO_Port GPIOB
#define i1_5_Pin GPIO_PIN_15
#define i1_5_GPIO_Port GPIOB
#define ms1_5_Pin GPIO_PIN_6
#define ms1_5_GPIO_Port GPIOC
#define dir_4_Pin GPIO_PIN_7
#define dir_4_GPIO_Port GPIOC
#define enbl_4_Pin GPIO_PIN_8
#define enbl_4_GPIO_Port GPIOC
#define ms2_4_Pin GPIO_PIN_9
#define ms2_4_GPIO_Port GPIOC
#define ms1_4_Pin GPIO_PIN_8
#define ms1_4_GPIO_Port GPIOA
#define i2_4_Pin GPIO_PIN_9
#define i2_4_GPIO_Port GPIOA
#define i1_4_Pin GPIO_PIN_10
#define i1_4_GPIO_Port GPIOA
#define i2_0_Pin GPIO_PIN_15
#define i2_0_GPIO_Port GPIOA
#define i1_0_Pin GPIO_PIN_10
#define i1_0_GPIO_Port GPIOC
#define ms2_0_Pin GPIO_PIN_11
#define ms2_0_GPIO_Port GPIOC
#define ms1_0_Pin GPIO_PIN_12
#define ms1_0_GPIO_Port GPIOC
#define dir_0_Pin GPIO_PIN_2
#define dir_0_GPIO_Port GPIOD
#define step_0_Pin GPIO_PIN_3
#define step_0_GPIO_Port GPIOB
#define enbl_0_Pin GPIO_PIN_4
#define enbl_0_GPIO_Port GPIOB
#define step_1_Pin GPIO_PIN_5
#define step_1_GPIO_Port GPIOB
#define dir_1_Pin GPIO_PIN_6
#define dir_1_GPIO_Port GPIOB
#define i2_1_Pin GPIO_PIN_7
#define i2_1_GPIO_Port GPIOB
#define step_2_Pin GPIO_PIN_8
#define step_2_GPIO_Port GPIOB
#define i1_1_Pin GPIO_PIN_9
#define i1_1_GPIO_Port GPIOB

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
