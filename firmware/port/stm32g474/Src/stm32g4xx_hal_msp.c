/* Низкоуровневая инициализация периферии: тактирование, выводы, прерывания. */
#include "stm32g4xx_hal.h"

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *h)
{
    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_PULLDOWN; g.Speed = GPIO_SPEED_FREQ_HIGH;
    if (h->Instance == TIM1) {
        __HAL_RCC_TIM1_CLK_ENABLE(); __HAL_RCC_GPIOA_CLK_ENABLE();
        g.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10; g.Alternate = GPIO_AF6_TIM1;
        HAL_GPIO_Init(GPIOA, &g);
    } else if (h->Instance == TIM8) {
        __HAL_RCC_TIM8_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();
        g.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8; g.Alternate = GPIO_AF4_TIM8;
        HAL_GPIO_Init(GPIOC, &g);
    }
}

void HAL_TIM_Encoder_MspInit(TIM_HandleTypeDef *h)
{
    GPIO_InitTypeDef g = {0};
    if (h->Instance == TIM4) {
        __HAL_RCC_TIM4_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE();
        g.Pin = GPIO_PIN_6 | GPIO_PIN_7; g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_PULLUP;
        g.Speed = GPIO_SPEED_FREQ_LOW; g.Alternate = GPIO_AF2_TIM4;
        HAL_GPIO_Init(GPIOB, &g);
    }
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *h)
{
    GPIO_InitTypeDef g = {0};
    if (h->Instance == ADC1) {
        RCC_PeriphCLKInitTypeDef pc = {0};
        pc.PeriphClockSelection = RCC_PERIPHCLK_ADC12;
        pc.Adc12ClockSelection = RCC_ADC12CLKSOURCE_SYSCLK;
        HAL_RCCEx_PeriphCLKConfig(&pc);
        __HAL_RCC_ADC12_CLK_ENABLE(); __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();
        g.Mode = GPIO_MODE_ANALOG; g.Pull = GPIO_NOPULL;
        g.Pin = GPIO_PIN_0 | GPIO_PIN_1; HAL_GPIO_Init(GPIOA, &g);
        g.Pin = GPIO_PIN_0 | GPIO_PIN_1; HAL_GPIO_Init(GPIOC, &g);
        HAL_NVIC_SetPriority(ADC1_2_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(ADC1_2_IRQn);
    }
}
