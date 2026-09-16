/*
 * Прошивка системы управления ВИД 8/6 для LAUNCHXL-F28379D (TMS320F28379D, C2000Ware driverlib).
 * Учебный проект дисциплины «Электромоторы», лабораторный стенд.
 *
 * Последовательность: калибровка нулей датчиков тока -> выставка ротора -> ожидание команды
 * g_start_request = 1 (из отладчика, окно Expressions) -> регулирование скорости.
 * Регулятор srm_ctrl_step выполняется в прерывании ADCA1 по окончании преобразования
 * последнего канала, запуск АЦП — от ePWM1 (SOCA) каждые 2 периода ШИМ, 10 кГц.
 * Сборка: проект Code Composer Studio с ядром ../../core и определением SRM_MOTOR_8_6.
 */
#include "driverlib.h"
#include "device.h"
#include "board.h"
#include "bsp.h"

void bsp_adc_sample(void);
void bsp_offset_accumulate(int reset, int n);
void bsp_set_theta_offset(void);
void bsp_pwm_on(void);
uint32_t bsp_ticks(void);
void bsp_store_isr_ticks(uint32_t start, uint32_t stop);

typedef enum { ST_OFFSETS = 0, ST_ALIGN, ST_READY, ST_RUN, ST_FAULT } state_t;

static const uint32_t EPWM_BASES[SRM_PHASES] = { EPWM1_BASE, EPWM2_BASE, EPWM3_BASE, EPWM4_BASE };
static srm_ctrl_t ctrl;

/* переменные для наблюдения и управления из отладчика */
volatile state_t  g_state = ST_OFFSETS;
volatile float    g_speed_ref_rpm = 500.0f;
volatile uint16_t g_start_request = 0U;
volatile float    g_isr_us = 0.0f, g_isr_us_max = 0.0f;
volatile float    g_n_rpm = 0.0f, g_ia = 0.0f, g_udc = 0.0f;
volatile uint16_t g_fault = 0U;

static void initEPWM(uint32_t base, int master);
static void initADC(void);
static void initEQEP(void);
static void initCpuTimer(void);
__interrupt void adcA1ISR(void);

void main(void)
{
    Device_init();                 /* тактирование 200 МГц, сторожевой таймер, периферия */
    Device_initGPIO();
    Interrupt_initModule();
    Interrupt_initVectorTable();

    /* выводы ШИМ и энкодера */
    GPIO_setPinConfig(GPIO_0_EPWM1A); GPIO_setPinConfig(GPIO_1_EPWM1B);
    GPIO_setPinConfig(GPIO_2_EPWM2A); GPIO_setPinConfig(GPIO_3_EPWM2B);
    GPIO_setPinConfig(GPIO_4_EPWM3A); GPIO_setPinConfig(GPIO_5_EPWM3B);
    GPIO_setPinConfig(GPIO_6_EPWM4A); GPIO_setPinConfig(GPIO_7_EPWM4B);
    GPIO_setPinConfig(GPIO_20_EQEP1A); GPIO_setPinConfig(GPIO_21_EQEP1B);
    GPIO_setPadConfig(20U, GPIO_PIN_TYPE_PULLUP);
    GPIO_setPadConfig(21U, GPIO_PIN_TYPE_PULLUP);
    GPIO_setPadConfig(DEVICE_GPIO_PIN_LED1, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_LED1, GPIO_DIR_MODE_OUT);

    Interrupt_register(INT_ADCA1, &adcA1ISR);

    /* ШИМ настраивается при остановленной синхронизации тактирования модулей */
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
    initEPWM(EPWM1_BASE, 1);
    initEPWM(EPWM2_BASE, 0);
    initEPWM(EPWM3_BASE, 0);
    initEPWM(EPWM4_BASE, 0);
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    initADC();
    initEQEP();
    initCpuTimer();

    srm_ctrl_init(&ctrl);
    ctrl.mode = SRM_MODE_SPEED;
    ctrl.cc = SRM_CC_MPC_LUT;
    ctrl.balance = 1;
    bsp_pwm_off();
    bsp_offset_accumulate(1, 0);

    /* запуск АЦП от ePWM1 и разрешение прерываний */
    EPWM_enableADCTrigger(EPWM1_BASE, EPWM_SOC_A);
    Interrupt_enable(INT_ADCA1);
    EINT;
    ERTM;

    for (;;) {
        GPIO_writePin(DEVICE_GPIO_PIN_LED1, (g_state == ST_RUN) ? 0U : 1U);   /* светодиод активен низким уровнем */
        DEVICE_DELAY_US(20000U);
    }
}

__interrupt void adcA1ISR(void)
{
    static uint32_t n = 0UL;
    uint32_t t0 = bsp_ticks();
    srm_meas_t m;
    srm_out_t o;
    uint16_t p;
    o.enable = 0;
    for (p = 0U; p < SRM_PHASES; p++) { o.duty_hi[p] = 0.0f; o.duty_lo[p] = 0.0f; o.u[p] = 0.0f; }

    bsp_adc_sample();
    n++;
    switch (g_state) {
    case ST_OFFSETS:                                  /* ключи закрыты, усреднение нулей АЦП */
        bsp_offset_accumulate(0, 0);
        if (n >= (uint32_t)(OFFSET_TIME_S / SRM_TS)) {
            bsp_offset_accumulate(0, (int)n + 1);
            n = 0UL;
            g_state = ST_ALIGN;
            bsp_pwm_on();
        }
        break;
    case ST_ALIGN:                                    /* постоянное напряжение на фазу A */
        bsp_read(&m);
        o.enable = 1;
        o.duty_hi[0] = ALIGN_DUTY;
        o.duty_lo[0] = 1.0f;
        if (m.i[0] > SRM_I_TRIP) {
            g_fault = SRM_FAULT_OVERCURRENT; g_state = ST_FAULT; o.enable = 0;
        } else if (n >= (uint32_t)(ALIGN_TIME_S / SRM_TS)) {
            o.enable = 0; bsp_set_theta_offset(); g_state = ST_READY;
        }
        break;
    case ST_READY:
        bsp_read(&m);
        if (g_start_request) {
            g_start_request = 0U;
            srm_ctrl_reset_fault(&ctrl);
            bsp_pwm_on();
            g_state = ST_RUN;
        }
        break;
    case ST_RUN:
        bsp_read(&m);
        ctrl.omega_ref = g_speed_ref_rpm * 6.2831853f / 60.0f;
        srm_ctrl_step(&ctrl, &m, &o);
        if (ctrl.fault) { g_fault = (uint16_t)ctrl.fault; g_state = ST_FAULT; }
        g_n_rpm = m.omega * 60.0f / 6.2831853f;
        g_ia = m.i[0];
        g_udc = m.udc;
        break;
    default:
        break;
    }
    bsp_write(&o);

    bsp_store_isr_ticks(t0, bsp_ticks());
    g_isr_us = bsp_isr_time_us();
    if (g_isr_us > g_isr_us_max) g_isr_us_max = g_isr_us;

    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    if (ADC_getInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1)) {
        ADC_clearInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1);
        ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    }
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

/* ШИМ с пилой вверх-вниз: выход A (верхний ключ) активен при счетчике ниже CMPA,
   выход B (нижний ключ) — ниже CMPB; импульсы обоих ключей центрированы относительно нуля счетчика.
   Мертвое время не нужно: ключи одной фазы несимметричного моста не образуют стойку. */
static void initEPWM(uint32_t base, int master)
{
    EPWM_setTimeBasePeriod(base, PWM_TBPRD);
    EPWM_setTimeBaseCounter(base, 0U);
    EPWM_setTimeBaseCounterMode(base, EPWM_COUNTER_MODE_UP_DOWN);
    EPWM_setClockPrescaler(base, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setPhaseShift(base, 0U);
    if (master) {
        EPWM_disablePhaseShiftLoad(base);
        EPWM_setSyncOutPulseMode(base, EPWM_SYNC_OUT_PULSE_ON_COUNTER_ZERO);
        EPWM_setADCTriggerSource(base, EPWM_SOC_A, EPWM_SOC_TBCTR_ZERO);
        EPWM_setADCTriggerEventPrescale(base, EPWM_SOC_A, SOC_PRESCALE);
    } else {
        EPWM_enablePhaseShiftLoad(base);
        EPWM_setSyncOutPulseMode(base, EPWM_SYNC_OUT_PULSE_ON_EPWMxSYNCIN);
    }
    EPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_A, EPWM_COMP_LOAD_ON_CNTR_ZERO);
    EPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_B, EPWM_COMP_LOAD_ON_CNTR_ZERO);
    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_A, 0U);
    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_B, 0U);

    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW,  EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_LOW,  EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPB);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPB);

    /* аварийное отключение: программное (однократное) и от входа TZ1 аппаратной защиты по току */
    EPWM_setTripZoneAction(base, EPWM_TZ_ACTION_EVENT_TZA, EPWM_TZ_ACTION_LOW);
    EPWM_setTripZoneAction(base, EPWM_TZ_ACTION_EVENT_TZB, EPWM_TZ_ACTION_LOW);
    EPWM_enableTripZoneSignals(base, EPWM_TZ_SIGNAL_OSHT1);
}

static void initADC(void)
{
    uint16_t k;
    ADC_setPrescaler(ADCA_BASE, ADC_CLK_DIV_4_0);
    ADC_setMode(ADCA_BASE, ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED);
    ADC_setInterruptPulseMode(ADCA_BASE, ADC_PULSE_END_OF_CONV);
    ADC_enableConverter(ADCA_BASE);
    DEVICE_DELAY_US(1000U);
    for (k = 0U; k <= SRM_PHASES; k++)          /* SOC0..SOC3 — токи A..D, SOC4 — напряжение звена */
        ADC_setupSOC(ADCA_BASE, (ADC_SOCNumber)k, ADC_TRIGGER_EPWM1_SOCA,
                     (ADC_Channel)k, ADC_ACQ_WINDOW);
    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, (ADC_SOCNumber)SRM_PHASES);
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
}

static void initEQEP(void)
{
    EQEP_setDecoderConfig(EQEP1_BASE, EQEP_CONFIG_QUADRATURE | EQEP_CONFIG_2X_RESOLUTION | EQEP_CONFIG_NO_SWAP);
    EQEP_setEmulationMode(EQEP1_BASE, EQEP_EMULATIONMODE_RUNFREE);
    EQEP_setPositionCounterConfig(EQEP1_BASE, EQEP_POSITION_RESET_MAX_POS, ENC_COUNTS - 1UL);
    EQEP_setPosition(EQEP1_BASE, 0UL);
    EQEP_enableModule(EQEP1_BASE);
}

static void initCpuTimer(void)
{
    CPUTimer_setPeriod(CPUTIMER1_BASE, 0xFFFFFFFFUL);
    CPUTimer_setPreScaler(CPUTIMER1_BASE, 0U);
    CPUTimer_disableInterrupt(CPUTIMER1_BASE);
    CPUTimer_setEmulationMode(CPUTIMER1_BASE, CPUTIMER_EMULATIONMODE_RUNFREE);
    CPUTimer_startTimer(CPUTIMER1_BASE);
}
