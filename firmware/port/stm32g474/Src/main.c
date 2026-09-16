/*
 * Прошивка системы управления ВИД для NUCLEO-G474RE (учебный проект дисциплины «Электромоторы»).
 * Последовательность: калибровка нулей датчиков тока -> выставка ротора -> ожидание кнопки ->
 * работа в режиме регулирования скорости. Регулятор srm_ctrl_step выполняется в прерывании
 * по окончании преобразования АЦП (10 кГц). Переменные g_* удобно наблюдать в отладчике
 * (Live Expressions в STM32CubeIDE).
 */
#include "stm32g4xx_hal.h"
#include "board.h"
#include "bsp.h"

ADC_HandleTypeDef hadc1;
TIM_HandleTypeDef htim1, htim8, htim4;

void bsp_adc_store(uint16_t ia, uint16_t ib, uint16_t ic, uint16_t udc);
void bsp_offset_accumulate(int reset, int n);
void bsp_set_theta_offset(void);
void bsp_timer_init(void);
uint32_t bsp_cycles(void);
void bsp_store_isr_cycles(uint32_t c);

typedef enum { ST_OFFSETS, ST_ALIGN, ST_READY, ST_RUN, ST_FAULT } state_t;

static srm_ctrl_t ctrl;
volatile state_t g_state = ST_OFFSETS;
volatile float   g_speed_ref_rpm = 500.0f;     /* задание скорости, изменяется из отладчика */
volatile float   g_isr_us = 0.0f, g_isr_us_max = 0.0f;
volatile float   g_n_rpm = 0.0f, g_ia = 0.0f, g_udc = 0.0f;
volatile int     g_fault = 0;
volatile int     g_start_request = 0;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM8_Init(void);
static void MX_TIM4_Init(void);
static void MX_ADC1_Init(void);
void Error_Handler(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM4_Init();
    MX_TIM8_Init();
    MX_TIM1_Init();
    MX_ADC1_Init();
    bsp_timer_init();

    srm_ctrl_init(&ctrl);
    ctrl.mode = SRM_MODE_SPEED;
    ctrl.cc = SRM_CC_MPC_LUT;
    ctrl.balance = 1;

    bsp_pwm_off();
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
    for (uint32_t ch = TIM_CHANNEL_1; ch <= TIM_CHANNEL_3; ch += 4u) {
        HAL_TIM_PWM_Start(&htim8, ch);            /* TIM8 запускается синхронно с TIM1 */
        HAL_TIM_PWM_Start(&htim1, ch);
    }
    bsp_pwm_off();
    bsp_offset_accumulate(1, 0);
    HAL_ADCEx_InjectedStart_IT(&hadc1);

    for (;;) {
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == BTN_ACTIVE_LEVEL) g_start_request = 1;
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, g_state == ST_RUN ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_Delay(20);
    }
}

/* Прерывание по окончании преобразования группы injected: основной цикл управления */
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    static uint32_t n = 0;
    uint32_t c0 = bsp_cycles();
    bsp_adc_store((uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1),
                  (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_2),
                  (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_3),
                  (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_4));
    srm_meas_t m;
    srm_out_t o = { .enable = 0 };
    n++;
    switch (g_state) {
    case ST_OFFSETS:                              /* ключи закрыты, усреднение нулей АЦП */
        bsp_offset_accumulate(0, 0);
        if (n >= (uint32_t)(OFFSET_TIME_S / SRM_TS)) {
            bsp_offset_accumulate(0, (int)n + 1);
            n = 0; g_state = ST_ALIGN;
        }
        break;
    case ST_ALIGN:                                /* постоянное напряжение на фазу A */
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
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    g.Pin = GPIO_PIN_5; g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_13; g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &g);
}

static void pwm_timer_init(TIM_HandleTypeDef *h, TIM_TypeDef *inst)
{
    TIM_OC_InitTypeDef oc = {0};
    TIM_BreakDeadTimeConfigTypeDef bdt = {0};
    h->Instance = inst;
    h->Init.Prescaler = 0;
    h->Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;
    h->Init.Period = PWM_ARR;
    h->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    h->Init.RepetitionCounter = 3;               /* обновление каждые 2 периода ШИМ: 10 кГц */
    h->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(h) != HAL_OK) Error_Handler();
    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    oc.OCIdleState = TIM_OCIDLESTATE_RESET;
    oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    for (uint32_t ch = TIM_CHANNEL_1; ch <= TIM_CHANNEL_3; ch += 4u)
        if (HAL_TIM_PWM_ConfigChannel(h, &oc, ch) != HAL_OK) Error_Handler();
    /* в несимметричном мосте ключи одной фазы не образуют стойку: мертвое время не требуется */
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
}

static void MX_TIM1_Init(void)
{
    TIM_MasterConfigTypeDef ms = {0};
    pwm_timer_init(&htim1, TIM1);
    ms.MasterOutputTrigger = TIM_TRGO_ENABLE;     /* запуск TIM8 одновременно с TIM1 */
    ms.MasterOutputTrigger2 = TIM_TRGO2_UPDATE;   /* запуск АЦП по событию обновления */
    ms.MasterSlaveMode = TIM_MASTERSLAVEMODE_ENABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &ms) != HAL_OK) Error_Handler();
}

static void MX_TIM8_Init(void)
{
    TIM_SlaveConfigTypeDef sl = {0};
    pwm_timer_init(&htim8, TIM8);
    sl.SlaveMode = TIM_SLAVEMODE_TRIGGER;
    sl.InputTrigger = TIM_TS_ITR0;                /* ITR0 таймера TIM8 — TIM1 (проверить в справочнике) */
    if (HAL_TIM_SlaveConfigSynchro(&htim8, &sl) != HAL_OK) Error_Handler();
}

static void MX_TIM4_Init(void)
{
    TIM_Encoder_InitTypeDef e = {0};
    htim4.Instance = TIM4;
    htim4.Init.Prescaler = 0;
    htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim4.Init.Period = ENC_COUNTS - 1u;
    htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    e.EncoderMode = TIM_ENCODERMODE_TI12;
    e.IC1Polarity = TIM_ICPOLARITY_RISING; e.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    e.IC1Prescaler = TIM_ICPSC_DIV1; e.IC1Filter = 4;
    e.IC2Polarity = TIM_ICPOLARITY_RISING; e.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    e.IC2Prescaler = TIM_ICPSC_DIV1; e.IC2Filter = 4;
    if (HAL_TIM_Encoder_Init(&htim4, &e) != HAL_OK) Error_Handler();
}

static void MX_ADC1_Init(void)
{
    ADC_InjectionConfTypeDef inj = {0};
    static const uint32_t chan[4] = { ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_6, ADC_CHANNEL_7 };
    static const uint32_t rank[4] = { ADC_INJECTED_RANK_1, ADC_INJECTED_RANK_2,
                                      ADC_INJECTED_RANK_3, ADC_INJECTED_RANK_4 };
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.GainCompensation = 0;
    hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    hadc1.Init.OversamplingMode = DISABLE;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();
    for (int k = 0; k < 4; k++) {
        inj.InjectedChannel = chan[k];
        inj.InjectedRank = rank[k];
        inj.InjectedSamplingTime = ADC_SAMPLETIME_6CYCLES_5;
        inj.InjectedSingleDiff = ADC_SINGLE_ENDED;
        inj.InjectedOffsetNumber = ADC_OFFSET_NONE;
        inj.InjectedOffset = 0;
        inj.InjectedNbrOfConversion = 4;
        inj.InjectedDiscontinuousConvMode = DISABLE;
        inj.AutoInjectedConv = DISABLE;
        inj.QueueInjectedContext = DISABLE;
        inj.ExternalTrigInjecConv = ADC_EXTERNALTRIGINJEC_T1_TRGO2;
        inj.ExternalTrigInjecConvEdge = ADC_EXTERNALTRIGINJECCONV_EDGE_RISING;
        inj.InjecOversamplingMode = DISABLE;
        if (HAL_ADCEx_InjectedConfigChannel(&hadc1, &inj) != HAL_OK) Error_Handler();
    }
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
    bsp_pwm_off();
    __disable_irq();
    for (;;) { }
}
