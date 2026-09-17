/*
 * Прошивка системы управления ВИД 8/6 для платы B-G473E-ZEST1S (STM32G473QET6).
 * Учебный проект дисциплины «Электромоторы».
 *
 * Последовательность: калибровка нулей датчиков тока -> выставка ротора -> ожидание нажатия
 * кнопки B2 -> регулирование скорости. Регулятор srm_ctrl_step выполняется в прерывании по
 * окончании преобразования группы injected АЦП1 (10 кГц). Переменные g_* наблюдаются в
 * отладчике (Live Expressions в STM32CubeIDE). Назначение выводов — в Inc/board.h.
 */
#include "stm32g4xx_hal.h"
#include "board.h"
#include "bsp.h"

ADC_HandleTypeDef hadc1, hadc2;
TIM_HandleTypeDef htim1, htim8, htim20, htim5;

void bsp_adc_store(const uint16_t *i, uint16_t udc);
void bsp_offset_accumulate(int reset, int n);
void bsp_set_theta_offset(void);
void bsp_timer_init(void);
uint32_t bsp_cycles(void);
void bsp_store_isr_cycles(uint32_t c);

typedef enum { ST_OFFSETS, ST_ALIGN, ST_READY, ST_RUN, ST_FAULT } state_t;

static srm_ctrl_t ctrl;
volatile state_t g_state = ST_OFFSETS;
volatile float   g_speed_ref_rpm = 500.0f;
volatile float   g_isr_us = 0.0f, g_isr_us_max = 0.0f;
volatile float   g_n_rpm = 0.0f, g_ia = 0.0f, g_udc = 0.0f;
volatile int     g_fault = 0;
volatile int     g_start_request = 0;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void pwm_timer_init(TIM_HandleTypeDef *h, TIM_TypeDef *inst, int n_ch, uint32_t rcr);
static void MX_TIM5_Init(void);
static void MX_ADC_Init(void);
static void pwm_start_synchronized(void);
void Error_Handler(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM5_Init();
    pwm_timer_init(&htim8, TIM8, 3, 0);
    pwm_timer_init(&htim20, TIM20, 2, 0);
    pwm_timer_init(&htim1, TIM1, 3, 3);          /* TIM1 задает запуск АЦП: 10 кГц */
    MX_ADC_Init();
    bsp_timer_init();

    srm_ctrl_init(&ctrl);
    ctrl.mode = SRM_MODE_SPEED;
    ctrl.cc = SRM_CC_MPC_LUT;
    ctrl.balance = 1;

    bsp_pwm_off();
    HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL);
    pwm_start_synchronized();
    bsp_pwm_off();
    bsp_offset_accumulate(1, 0);
    HAL_ADCEx_InjectedStart(&hadc2);
    HAL_ADCEx_InjectedStart_IT(&hadc1);

    for (;;) {
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == BTN_ACTIVE_LEVEL) g_start_request = 1;
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_3, g_state == ST_RUN ? GPIO_PIN_RESET : GPIO_PIN_SET);
        HAL_Delay(20);
    }
}

/* Прерывание по окончании преобразования группы injected АЦП1: основной цикл управления */
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    static uint32_t n = 0;
    if (hadc->Instance != ADC1) return;
    uint32_t c0 = bsp_cycles();
    uint16_t ri[SRM_PHASES];
    static const uint32_t rank[SRM_PHASES] = { ADC_INJECTED_RANK_1, ADC_INJECTED_RANK_2,
                                               ADC_INJECTED_RANK_3, ADC_INJECTED_RANK_4 };
    for (int k = 0; k < SRM_PHASES; k++) ri[k] = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, rank[k]);
    bsp_adc_store(ri, (uint16_t)HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_1));

    srm_meas_t m;
    srm_out_t o = { .enable = 0 };
    n++;
    switch (g_state) {
    case ST_OFFSETS:
        bsp_offset_accumulate(0, 0);
        if (n >= (uint32_t)(OFFSET_TIME_S / SRM_TS)) {
            bsp_offset_accumulate(0, (int)n + 1);
            n = 0; g_state = ST_ALIGN;
        }
        break;
    case ST_ALIGN:
        o.enable = 1;
        o.duty_hi[0] = ALIGN_DUTY; o.duty_lo[0] = 1.0f;
        bsp_read(&m);
        if (m.i[0] > SRM_I_TRIP) { g_fault = SRM_FAULT_OVERCURRENT; g_state = ST_FAULT; o.enable = 0; }
        else if (n >= (uint32_t)(ALIGN_TIME_S / SRM_TS)) {
            o.enable = 0; bsp_set_theta_offset(); g_state = ST_READY;
        }
        break;
    case ST_READY:
        bsp_read(&m);
        if (g_start_request) { g_start_request = 0; srm_ctrl_reset_fault(&ctrl); g_state = ST_RUN; }
        break;
    case ST_RUN:
        bsp_read(&m);
        ctrl.omega_ref = g_speed_ref_rpm * 6.2831853f / 60.0f;
        srm_ctrl_step(&ctrl, &m, &o);
        if (ctrl.fault) { g_fault = ctrl.fault; g_state = ST_FAULT; }
        g_n_rpm = m.omega * 60.0f / 6.2831853f; g_ia = m.i[0]; g_udc = m.udc;
        break;
    default:
        break;
    }
    bsp_write(&o);
    bsp_store_isr_cycles(bsp_cycles() - c0);
    g_isr_us = bsp_isr_time_us();
    if (g_isr_us > g_isr_us_max) g_isr_us_max = g_isr_us;
}

/* ---------------- инициализация периферии ---------------- */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = RCC_PLLM_DIV4;                 /* 16 / 4 * 85 / 2 = 170 МГц */
    osc.PLL.PLLN = 85;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = RCC_PLLQ_DIV2;
    osc.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();
    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_3, GPIO_PIN_SET);                   /* LD1 погашен */
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);    /* силовой модуль запрещен */
    g.Pin = GPIO_PIN_3; g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOF, &g);
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7; g.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOG, &g);
    g.Pin = GPIO_PIN_13; g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &g);
}

static void pwm_timer_init(TIM_HandleTypeDef *h, TIM_TypeDef *inst, int n_ch, uint32_t rcr)
{
    TIM_OC_InitTypeDef oc = {0};
    TIM_BreakDeadTimeConfigTypeDef bdt = {0};
    TIM_MasterConfigTypeDef ms = {0};
    h->Instance = inst;
    h->Init.Prescaler = 0;
    h->Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;
    h->Init.Period = PWM_ARR;
    h->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    h->Init.RepetitionCounter = rcr;
    h->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(h) != HAL_OK) Error_Handler();
    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    oc.OCIdleState = TIM_OCIDLESTATE_RESET;
    oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    static const uint32_t chs[3] = { TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3 };
    for (int k = 0; k < n_ch; k++)
        if (HAL_TIM_PWM_ConfigChannel(h, &oc, chs[k]) != HAL_OK) Error_Handler();
    /* мертвое время не требуется: ключи одной фазы несимметричного моста не образуют стойку.
       Входы аварийного отключения (BKIN) подключаются после проверки полярности сигнала
       силового модуля. */
    bdt.OffStateRunMode = TIM_OSSR_ENABLE;
    bdt.OffStateIDLEMode = TIM_OSSI_ENABLE;
    bdt.LockLevel = TIM_LOCKLEVEL_OFF;
    bdt.DeadTime = 0;
    bdt.BreakState = TIM_BREAK_DISABLE;
    bdt.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
    bdt.BreakFilter = 0;
    bdt.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
    bdt.Break2State = TIM_BREAK2_DISABLE;
    bdt.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
    bdt.Break2Filter = 0;
    bdt.Break2AFMode = TIM_BREAK_AFMODE_INPUT;
    bdt.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    if (HAL_TIMEx_ConfigBreakDeadTime(h, &bdt) != HAL_OK) Error_Handler();
    ms.MasterOutputTrigger = TIM_TRGO_RESET;
    ms.MasterOutputTrigger2 = (inst == TIM1) ? TIM_TRGO2_UPDATE : TIM_TRGO2_RESET;
    ms.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(h, &ms) != HAL_OK) Error_Handler();
}

/* Одновременный пуск трех таймеров ШИМ: счетчики обнуляются и запускаются подряд
   при запрещенных прерываниях; расхождение фаз составляет единицы тактов из 4250. */
static void pwm_start_synchronized(void)
{
    static const uint32_t chs[3] = { TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3 };
    for (int k = 0; k < 3; k++) {
        HAL_TIM_PWM_Start(&htim8, chs[k]);
        HAL_TIM_PWM_Start(&htim1, chs[k]);
    }
    HAL_TIM_PWM_Start(&htim20, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim20, TIM_CHANNEL_2);
    __disable_irq();
    TIM1->CR1 &= ~TIM_CR1_CEN; TIM8->CR1 &= ~TIM_CR1_CEN; TIM20->CR1 &= ~TIM_CR1_CEN;
    TIM1->CNT = 0; TIM8->CNT = 0; TIM20->CNT = 0;
    TIM1->CR1 |= TIM_CR1_CEN; TIM8->CR1 |= TIM_CR1_CEN; TIM20->CR1 |= TIM_CR1_CEN;
    __enable_irq();
}

static void MX_TIM5_Init(void)
{
    TIM_Encoder_InitTypeDef e = {0};
    htim5.Instance = TIM5;
    htim5.Init.Prescaler = 0;
    htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim5.Init.Period = ENC_COUNTS - 1u;
    htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    e.EncoderMode = TIM_ENCODERMODE_TI12;
    e.IC1Polarity = TIM_ICPOLARITY_RISING; e.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    e.IC1Prescaler = TIM_ICPSC_DIV1; e.IC1Filter = 4;
    e.IC2Polarity = TIM_ICPOLARITY_RISING; e.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    e.IC2Prescaler = TIM_ICPSC_DIV1; e.IC2Filter = 4;
    if (HAL_TIM_Encoder_Init(&htim5, &e) != HAL_OK) Error_Handler();
}

static void adc_base_init(ADC_HandleTypeDef *h, ADC_TypeDef *inst)
{
    h->Instance = inst;
    h->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    h->Init.Resolution = ADC_RESOLUTION_12B;
    h->Init.DataAlign = ADC_DATAALIGN_RIGHT;
    h->Init.GainCompensation = 0;
    h->Init.ScanConvMode = ADC_SCAN_ENABLE;
    h->Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    h->Init.LowPowerAutoWait = DISABLE;
    h->Init.ContinuousConvMode = DISABLE;
    h->Init.NbrOfConversion = 1;
    h->Init.DiscontinuousConvMode = DISABLE;
    h->Init.ExternalTrigConv = ADC_SOFTWARE_START;
    h->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    h->Init.DMAContinuousRequests = DISABLE;
    h->Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    h->Init.OversamplingMode = DISABLE;
    if (HAL_ADC_Init(h) != HAL_OK) Error_Handler();
}

static void adc_injected(ADC_HandleTypeDef *h, uint32_t ch, uint32_t rank, uint32_t n)
{
    ADC_InjectionConfTypeDef inj = {0};
    inj.InjectedChannel = ch;
    inj.InjectedRank = rank;
    inj.InjectedSamplingTime = ADC_SAMPLETIME_6CYCLES_5;
    inj.InjectedSingleDiff = ADC_SINGLE_ENDED;
    inj.InjectedOffsetNumber = ADC_OFFSET_NONE;
    inj.InjectedOffset = 0;
    inj.InjectedNbrOfConversion = n;
    inj.InjectedDiscontinuousConvMode = DISABLE;
    inj.AutoInjectedConv = DISABLE;
    inj.QueueInjectedContext = DISABLE;
    inj.ExternalTrigInjecConv = ADC_EXTERNALTRIGINJEC_T1_TRGO2;
    inj.ExternalTrigInjecConvEdge = ADC_EXTERNALTRIGINJECCONV_EDGE_RISING;
    inj.InjecOversamplingMode = DISABLE;
    if (HAL_ADCEx_InjectedConfigChannel(h, &inj) != HAL_OK) Error_Handler();
}

static void MX_ADC_Init(void)
{
    adc_base_init(&hadc1, ADC1);
    adc_base_init(&hadc2, ADC2);
    adc_injected(&hadc1, ADC_CHANNEL_8, ADC_INJECTED_RANK_1, 4);   /* PC2: ток фазы A */
    adc_injected(&hadc1, ADC_CHANNEL_9, ADC_INJECTED_RANK_2, 4);   /* PC3: ток фазы B */
    adc_injected(&hadc1, ADC_CHANNEL_2, ADC_INJECTED_RANK_3, 4);   /* PA1: ток фазы C */
    adc_injected(&hadc1, ADC_CHANNEL_3, ADC_INJECTED_RANK_4, 4);   /* PA2: ток фазы D */
    adc_injected(&hadc2, ADC_CHANNEL_6, ADC_INJECTED_RANK_1, 1);   /* PC0: напряжение звена */
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK) Error_Handler();
    if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
    bsp_pwm_off();
    __disable_irq();
    for (;;) { }
}
