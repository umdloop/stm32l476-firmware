
#include "main.h"

/* Platform */
#include "../../Platform/Inc/system_clock.h"
#include "../../Platform/Inc/gpio.h"
#include "../../Platform/Inc/tim.h"

/* ===== MOTOR COUNT (YOU HAVE 6 CONFIGURED) ===== */
#define MOTOR_COUNT 6

/* DIR pins */
GPIO_TypeDef* DIR_PORT[MOTOR_COUNT] = {
    GPIOD, GPIOB, GPIOC, GPIOA, GPIOC, GPIOB
};

uint16_t DIR_PIN[MOTOR_COUNT] = {
    GPIO_PIN_2,   // dir_0
    GPIO_PIN_6,   // dir_1
    GPIO_PIN_14,  // dir_2
    GPIO_PIN_2,   // dir_3
    GPIO_PIN_7,   // dir_4
    GPIO_PIN_2    // dir_5
};

/* ENABLE pins (active LOW) */
GPIO_TypeDef* EN_PORT[MOTOR_COUNT] = {
    GPIOB, GPIOC, GPIOA, GPIOC, GPIOC, GPIOB
};

uint16_t EN_PIN[MOTOR_COUNT] = {
    GPIO_PIN_4,   // enbl_0
    GPIO_PIN_13,  // enbl_1
    GPIO_PIN_3,   // enbl_2
    GPIO_PIN_4,   // enbl_3
    GPIO_PIN_8,   // enbl_4
    GPIO_PIN_14   // enbl_5
};

static void enable_all(void)
{
    for (int i = 0; i < MOTOR_COUNT; i++)
        HAL_GPIO_WritePin(EN_PORT[i], EN_PIN[i], GPIO_PIN_RESET); // LOW = enable
}

static void set_direction(uint8_t dir)
{
    for (int i = 0; i < MOTOR_COUNT; i++)
        HAL_GPIO_WritePin(DIR_PORT[i], DIR_PIN[i],
                          dir ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void start_pwm(void)
{
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1); // step_3
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2); // step_0
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3); // step_5

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2); // step_1
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3); // step_4

    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3); // step_2
}

static void set_50_percent_duty(void)
{
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 500);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 500);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 500);

    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 500);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 500);

    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, 500);
}

int main(void)
{
    HAL_Init();
    Platform_SystemClock_Config();

    MX_GPIO_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();
    MX_TIM4_Init();
    start_pwm();

    set_50_percent_duty();
    enable_all();

    TIM2 -> CCR1 = 500;


    while (1)
    {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(1000);
    }
}

