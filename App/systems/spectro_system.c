#include "spectro_system.h"
#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#define SPECTRO_TIMER_CLK_HZ     8000000u

#define TCD_FM_HZ                2000000u
#define TCD_ICG_HZ               133u        // approximate label only
#define TCD_ICG_PERIOD_TICKS     60000u     // 7.5 ms at 80 MHz
#define TCD_ICG_PULSE_TICKS      80u        // 10 us at 80 MHz
#define TCD_SH_PERIOD_TICKS      160u       // 20 us at 80 MHz
#define TCD_SH_PULSE_TICKS       32u        // 4 us at 80 MHz

static bool s_initialized = false;

static void spectro_gpio_config_af(GPIO_TypeDef *gpio,
                                   uint32_t pin,
                                   uint32_t af)
{
    uint32_t afr_index = pin / 8u;
    uint32_t afr_shift = (pin % 8u) * 4u;
    uint32_t moder_shift = pin * 2u;
    uint32_t speed_shift = pin * 2u;
    uint32_t pupd_shift = pin * 2u;

    // Alternate-function mode: MODER = 10
    gpio->MODER &= ~(3u << moder_shift);
    gpio->MODER |=  (2u << moder_shift);

    // Push-pull output
    gpio->OTYPER &= ~(1u << pin);

    // Very high speed
    gpio->OSPEEDR &= ~(3u << speed_shift);
    gpio->OSPEEDR |=  (3u << speed_shift);

    // No pull-up/pull-down
    gpio->PUPDR &= ~(3u << pupd_shift);

    // Alternate function selection
    gpio->AFR[afr_index] &= ~(0xFu << afr_shift);
    gpio->AFR[afr_index] |=  (af   << afr_shift);
}

static void spectro_gpio_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;

    // PA5 = TIM2_CH1 = ICG = AF1
    spectro_gpio_config_af(GPIOA, 5u, 1u);

    // PA6 = TIM3_CH1 = fM = AF2
    spectro_gpio_config_af(GPIOA, 6u, 2u);

    // PC8 = TIM8_CH3 = SH = AF3
    spectro_gpio_config_af(GPIOC, 8u, 3u);
}

static void spectro_timer_clocks_enable(void)
{
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;
    RCC->APB2ENR  |= RCC_APB2ENR_TIM8EN;

    // Small dummy reads to allow clock domains to settle
    (void)RCC->APB1ENR1;
    (void)RCC->APB2ENR;
}

static void tim_pwm_ch1_init(TIM_TypeDef *tim,
                             uint32_t arr,
                             uint32_t ccr,
                             bool active_low)
{
    tim->CR1 = 0;
    tim->PSC = 0;
    tim->ARR = arr - 1u;
    tim->CCR1 = ccr;

    // PWM mode 1 on CH1, preload enable
    tim->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_OC1PE);
    tim->CCMR1 |=  (6u << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE;

    // Enable CH1 output
    tim->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC1E);
    if (active_low)
    {
        tim->CCER |= TIM_CCER_CC1P;
    }
    tim->CCER |= TIM_CCER_CC1E;

    // Auto-reload preload
    tim->CR1 |= TIM_CR1_ARPE;

    // Force update event
    tim->EGR = TIM_EGR_UG;
}

static void tim_pwm_ch3_init(TIM_TypeDef *tim,
                             uint32_t arr,
                             uint32_t ccr,
                             bool active_low)
{
    tim->CR1 = 0;
    tim->PSC = 0;
    tim->ARR = arr - 1u;
    tim->CCR3 = ccr;

    // PWM mode 1 on CH3, preload enable
    tim->CCMR2 &= ~(TIM_CCMR2_OC3M | TIM_CCMR2_OC3PE);
    tim->CCMR2 |=  (6u << TIM_CCMR2_OC3M_Pos) | TIM_CCMR2_OC3PE;

    // Enable CH3 output
    tim->CCER &= ~(TIM_CCER_CC3P | TIM_CCER_CC3E);
    if (active_low)
    {
        tim->CCER |= TIM_CCER_CC3P;
    }
    tim->CCER |= TIM_CCER_CC3E;

    // TIM8 is an advanced timer; main output enable is required
    if (tim == TIM8)
    {
        tim->BDTR |= TIM_BDTR_MOE;
    }

    tim->CR1 |= TIM_CR1_ARPE;
    tim->EGR = TIM_EGR_UG;
}

static void spectro_timers_init(void)
{
    /*
     * TIM3_CH1: fM on PA6
     * 80 MHz / 40 = 2 MHz
     */
    tim_pwm_ch1_init(TIM3,
                     4u,
                     2u,
                     false);

    /*
     * TIM2_CH1: ICG on PA5
     * 80 MHz / 600000 = 133.333 Hz
     * 800 ticks = 10 us pulse
     *
     * MCU pin is active high.
     * After 74HC04 inverter, CCD sees low-going ICG pulse.
     */
    tim_pwm_ch1_init(TIM2,
                     TCD_ICG_PERIOD_TICKS,
                     TCD_ICG_PULSE_TICKS,
                     false);

    /*
     * TIM2 master trigger.
     *
     * CubeMX used TIM_TRGO_ENABLE.
     * For repeated synchronization of SH to each ICG frame, UPDATE is usually
     * the more useful trigger source.
     */
    TIM2->CR2 &= ~TIM_CR2_MMS;
    TIM2->CR2 |=  (2u << TIM_CR2_MMS_Pos);   // MMS = 010: update event as TRGO

    /*
     * TIM8_CH3: SH on PC8
     * 320 ticks at 80 MHz = 4 us
     *
     * MCU pin is active low.
     * After 74HC04 inverter, CCD sees active-high SH pulse.
     */
    tim_pwm_ch3_init(TIM8,
                     TCD_SH_PERIOD_TICKS,
                     TCD_SH_PULSE_TICKS,
                     true);

    /*
     * TIM8 slave trigger selection.
     *
     * CubeMX used:
     *   SlaveMode = TIM_SLAVEMODE_TRIGGER
     *   InputTrigger = TIM_TS_ITR1
     *
     * Register meaning:
     *   TS  = ITR1
     *   SMS = Trigger mode
     */
    TIM8->SMCR &= ~(TIM_SMCR_TS | TIM_SMCR_SMS);
    TIM8->SMCR |=  (1u << TIM_SMCR_TS_Pos);  // TS = ITR1
    TIM8->SMCR |=  (6u << TIM_SMCR_SMS_Pos); // SMS = trigger mode

    /*
     * Optional phase tweak from Spectro.zip:
     *   __HAL_TIM_SET_COUNTER(&htim2, 66);
     */
    TIM2->CNT = 66u;
}

static void spectro_timers_start(void)
{
    /*
     * Start order copied conceptually from Spectro.zip:
     *
     * HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
     * HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
     * HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_3);
     */
    TIM3->CR1 |= TIM_CR1_CEN;
    TIM8->CR1 |= TIM_CR1_CEN;
    TIM2->CR1 |= TIM_CR1_CEN;
}

bool spectro_system_init(void)
{
    if (s_initialized)
    {
        return true;
    }

    spectro_gpio_init();
    spectro_timer_clocks_enable();
    spectro_timers_init();
    spectro_timers_start();

    s_initialized = true;
    return true;
}

void spectro_system_controller(void)
{
    if (!s_initialized)
    {
        (void)spectro_system_init();
    }

    /*
     * For pure timing bring-up, nothing is needed here.
     * The timers run in hardware once initialized.
     */
}
