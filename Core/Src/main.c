/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Motherboard firmware entrypoint (mandatory init + round-robin)
  ******************************************************************************
  * Rules for this file:
  * - Only mandatory initialization (clock + MX_*_Init calls)
  * - Enable/disable high-level systems
  * - Run the round-robin scheduler
  * - NO protocol logic here
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

#include "gpio.h"
#include "can.h"
#include "usart.h"

#include "app_config.h"
#include "rr_scheduler.h"

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void Error_Handler(void);

/* Private user code ---------------------------------------------------------*/
int main(void)
{
  HAL_Init();

  SystemClock_Config();

  /* Mandatory peripheral initializations (NO protocols here) */
  MX_GPIO_Init();
  MX_CAN1_Init();
  MX_UART4_Init();

  /* Round-robin init */
  RR_Scheduler_Init();

  while (1)
  {
    RR_Scheduler_Tick();
  }
}

/**
  * @brief System Clock Configuration
  *
  * Assumes motherboard uses external clock via HSE bypass.
  * Adjust HSE settings in the .ioc if you change hardware.
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /* Enable power control clock */
  __HAL_RCC_PWR_CLK_ENABLE();

  /* Voltage scaling: default */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /* HSE bypass as system clock source (no PLL here yet) */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
