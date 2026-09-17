/* Низкоуровневая инициализация периферии B-G473E-ZEST1S: тактирование, выводы, прерывания. */
#include "stm32g4xx_hal.h"

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

static void af_pins(GPIO_TypeDef *port, uint32_t pins, uint8_t af)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = pins; g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_PULLDOWN;
    g.Speed = GPIO_SPEED_FREQ_HIGH; g.Alternate = af;
    HAL_GPIO_Init(port, &g);
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *h)
{
    if (h->Instance == TIM8) {              /* верхние ключи фаз A, B, C */
        __HAL_RCC_TIM8_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();
        af_pins(GPIOC, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8, GPIO_AF4_TIM8);
    } else if (h->Instance == TIM1) {       /* нижние ключи фаз A, B, C */
        __HAL_RCC_TIM1_CLK_ENABLE(); __HAL_RCC_GPIOE_CLK_ENABLE();
        af_pins(GPIOE, GPIO_PIN_9 | GPIO_PIN_11 | GPIO_PIN_13, GPIO_AF2_TIM1);
    } else if (h->Instance == TIM20) {      /* фаза D: верхний и нижний ключи */
        __HAL_RCC_TIM20_CLK_ENABLE(); __HAL_RCC_GPIOF_CLK_ENABLE();
        af_pins(GPIOF, GPIO_PIN_12 | GPIO_PIN_13, GPIO_AF2_TIM20);
    }
}

void HAL_TIM_Encoder_MspInit(TIM_HandleTypeDef *h)
{
    GPIO_InitTypeDef g = {0};
    if (h->Instance == TIM5) {
        __HAL_RCC_TIM5_CLK_ENABLE(); __HAL_RCC_GPIOF_CLK_ENABLE();
        g.Pin = GPIO_PIN_6 | GPIO_PIN_7; g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_PULLUP;
        g.Speed = GPIO_SPEED_FREQ_LOW; g.Alternate = GPIO_AF6_TIM5;
        HAL_GPIO_Init(GPIOF, &g);
    }
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *h)
{
    static int clock_on = 0;
    GPIO_InitTypeDef g = {0};
    if (!clock_on) {
        RCC_PeriphCLKInitTypeDef pc = {0};
        pc.PeriphClockSelection = RCC_PERIPHCLK_ADC12;
        pc.Adc12ClockSelection = RCC_ADC12CLKSOURCE_SYSCLK;
        HAL_RCCEx_PeriphCLKConfig(&pc);
        __HAL_RCC_ADC12_CLK_ENABLE();
        clock_on = 1;
    }
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();
    g.Mode = GPIO_MODE_ANALOG; g.Pull = GPIO_NOPULL;
    if (h->Instance == ADC1) {
        g.Pin = GPIO_PIN_2 | GPIO_PIN_3; HAL_GPIO_Init(GPIOC, &g);      /* токи A, B */
        g.Pin = GPIO_PIN_1 | GPIO_PIN_2; HAL_GPIO_Init(GPIOA, &g);      /* токи C, D */
        HAL_NVIC_SetPriority(ADC1_2_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(ADC1_2_IRQn);
    } else if (h->Instance == ADC2) {
        g.Pin = GPIO_PIN_0; HAL_GPIO_Init(GPIOC, &g);                   /* напряжение звена */
    }
}
