#include "gpio.h"

void MX_GPIO_Init(void)
	{

	  GPIO_InitTypeDef GPIO_InitStruct = {0};

	  /* GPIO Ports Clock Enable */
	  __HAL_RCC_GPIOC_CLK_ENABLE();
	  __HAL_RCC_GPIOH_CLK_ENABLE();
	  __HAL_RCC_GPIOA_CLK_ENABLE();
	  __HAL_RCC_GPIOB_CLK_ENABLE();
	  __HAL_RCC_GPIOD_CLK_ENABLE();

	  /*Configure GPIO pin Output Level */
	  HAL_GPIO_WritePin(GPIOC, enbl_1_Pin|dir_2_Pin|i1_2_Pin|i2_2_Pin
							  |ms1_2_Pin|ms2_2_Pin|enbl_3_Pin|ms2_3_Pin
							  |ms1_5_Pin|dir_4_Pin|enbl_4_Pin|ms2_4_Pin
							  |i1_0_Pin|ms2_0_Pin|ms1_0_Pin, GPIO_PIN_RESET);

	  /*Configure GPIO pin Output Level */
	  HAL_GPIO_WritePin(ms2_1_GPIO_Port, ms2_1_Pin, GPIO_PIN_RESET);

	  /*Configure GPIO pin Output Level */
	  HAL_GPIO_WritePin(GPIOA, dir_3_Pin|enbl_2_Pin|i2_3_Pin|ms1_3_Pin
							  |i1_3_Pin|ms1_4_Pin|i2_4_Pin|i1_4_Pin
							  |i2_0_Pin, GPIO_PIN_RESET);

	  /*Configure GPIO pin Output Level */
	  HAL_GPIO_WritePin(GPIOB, dir_5_Pin|i2_5_Pin|ms2_5_Pin|enbl_5_Pin
							  |i1_5_Pin|enbl_0_Pin|dir_1_Pin|i2_1_Pin
							  |i1_1_Pin, GPIO_PIN_RESET);

	  /*Configure GPIO pin Output Level */
	  HAL_GPIO_WritePin(dir_0_GPIO_Port, dir_0_Pin, GPIO_PIN_RESET);

	  /*Configure GPIO pins : PCPin PCPin PCPin */
	  GPIO_InitStruct.Pin = enbl_1_Pin|enbl_3_Pin|enbl_4_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStruct.Pull = GPIO_PULLUP;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	  /*Configure GPIO pins : PCPin PCPin PCPin PCPin
							   PCPin PCPin PCPin PCPin
							   PCPin PCPin PCPin PCPin */
	  GPIO_InitStruct.Pin = dir_2_Pin|i1_2_Pin|i2_2_Pin|ms1_2_Pin
							  |ms2_2_Pin|ms2_3_Pin|ms1_5_Pin|dir_4_Pin
							  |ms2_4_Pin|i1_0_Pin|ms2_0_Pin|ms1_0_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	  /*Configure GPIO pin : PtPin */
	  GPIO_InitStruct.Pin = ms1_1_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	  HAL_GPIO_Init(ms1_1_GPIO_Port, &GPIO_InitStruct);

	  /*Configure GPIO pin : PtPin */
	  GPIO_InitStruct.Pin = ms2_1_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	  HAL_GPIO_Init(ms2_1_GPIO_Port, &GPIO_InitStruct);

	  /*Configure GPIO pins : PAPin PAPin PAPin PAPin
							   PAPin PAPin PAPin PAPin */
	  GPIO_InitStruct.Pin = dir_3_Pin|i2_3_Pin|ms1_3_Pin|i1_3_Pin
							  |ms1_4_Pin|i2_4_Pin|i1_4_Pin|i2_0_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	  /*Configure GPIO pin : PtPin */
	  GPIO_InitStruct.Pin = enbl_2_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStruct.Pull = GPIO_PULLUP;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	  HAL_GPIO_Init(enbl_2_GPIO_Port, &GPIO_InitStruct);

	  /*Configure GPIO pins : PBPin PBPin PBPin PBPin
							   PBPin PBPin PBPin */
	  GPIO_InitStruct.Pin = dir_5_Pin|i2_5_Pin|ms2_5_Pin|i1_5_Pin
							  |dir_1_Pin|i2_1_Pin|i1_1_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	  /*Configure GPIO pins : PBPin PBPin */
	  GPIO_InitStruct.Pin = enbl_5_Pin|enbl_0_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStruct.Pull = GPIO_PULLUP;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	  /*Configure GPIO pin : PtPin */
	  GPIO_InitStruct.Pin = dir_0_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	  HAL_GPIO_Init(dir_0_GPIO_Port, &GPIO_InitStruct);

	}
