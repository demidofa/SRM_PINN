/* Реализация интерфейса платы bsp.h для TMS320F28379D (C2000Ware driverlib). */
#include <math.h>
#include "driverlib.h"
#include "device.h"
#include "board.h"
#include "bsp.h"

static const uint32_t EPWM_BASES[SRM_PHASES] = { EPWM1_BASE, EPWM2_BASE, EPWM3_BASE, EPWM4_BASE };

static float adc_off[SRM_PHASES] = { 2048.0f, 2048.0f, 2048.0f, 2048.0f };
static float acc[SRM_PHASES];
static uint16_t raw[SRM_PHASES + 1];
static float theta_off = 0.0f;
static int32_t enc_prev = 0;
static float omega_f = 0.0f;
static float t_isr_us = 0.0f;

void bsp_adc_sample(void)
{
    uint16_t k;
    for (k = 0U; k <= SRM_PHASES; k++)
        raw[k] = ADC_readResult(ADCA_BASE, (ADC_SOCNumber)k);   /* SOC0..SOC4 */
}

void bsp_offset_accumulate(int reset, int n)
{
    uint16_t k;
    if (reset) { for (k = 0U; k < SRM_PHASES; k++) acc[k] = 0.0f; return; }
    for (k = 0U; k < SRM_PHASES; k++) acc[k] += (float)raw[k];
    if (n > 0) for (k = 0U; k < SRM_PHASES; k++) adc_off[k] = acc[k] / (float)n;
}

void bsp_set_theta_offset(void)
{
    /* после выставки ротор стоит в согласованном положении фазы A: электрический угол pi,
       механический угол pi / Nr */
    float th = (float)EQEP_getPosition(EQEP1_BASE) * 6.2831853f / (float)ENC_COUNTS;
    theta_off = th - 3.14159265f / (float)SRM_NR;
    enc_prev = (int32_t)EQEP_getPosition(EQEP1_BASE);
    omega_f = 0.0f;
}

void bsp_init(void) { }

void bsp_read(srm_meas_t *m)
{
    const float k = ADC_VREF / ADC_FS;
    uint16_t p;
    for (p = 0U; p < SRM_PHASES; p++) m->i[p] = ((float)raw[p] - adc_off[p]) * k * K_I_A_PER_V;
    m->udc = (float)raw[SRM_PHASES] * k * K_UDC_V_PER_V;

    int32_t cnt = (int32_t)EQEP_getPosition(EQEP1_BASE);
    int32_t d = cnt - enc_prev;
    if (d >  (int32_t)(ENC_COUNTS / 2UL)) d -= (int32_t)ENC_COUNTS;
    if (d < -(int32_t)(ENC_COUNTS / 2UL)) d += (int32_t)ENC_COUNTS;
    enc_prev = cnt;
    omega_f += 0.05f * ((float)d * 6.2831853f / (float)ENC_COUNTS / SRM_TS - omega_f);
    float th = fmodf((float)cnt * 6.2831853f / (float)ENC_COUNTS - theta_off, 6.2831853f);
    m->theta_mech = th < 0.0f ? th + 6.2831853f : th;
    m->omega = omega_f;
}

static uint16_t cmp(float d)
{
    if (d < 0.0f) d = 0.0f;
    if (d > 1.0f) d = 1.0f;
    return (uint16_t)(d * (float)PWM_TBPRD);
}

void bsp_write(const srm_out_t *o)
{
    uint16_t p;
    if (!o->enable) { bsp_pwm_off(); return; }
    for (p = 0U; p < SRM_PHASES; p++) {
        EPWM_setCounterCompareValue(EPWM_BASES[p], EPWM_COUNTER_COMPARE_A, cmp(o->duty_hi[p]));
        EPWM_setCounterCompareValue(EPWM_BASES[p], EPWM_COUNTER_COMPARE_B, cmp(o->duty_lo[p]));
    }
}

void bsp_pwm_off(void)
{
    uint16_t p;
    for (p = 0U; p < SRM_PHASES; p++) {
        EPWM_setCounterCompareValue(EPWM_BASES[p], EPWM_COUNTER_COMPARE_A, 0U);
        EPWM_setCounterCompareValue(EPWM_BASES[p], EPWM_COUNTER_COMPARE_B, 0U);
        EPWM_forceTripZoneEvent(EPWM_BASES[p], EPWM_TZ_FORCE_EVENT_OST);   /* выходы A и B в низкий уровень */
    }
}

void bsp_pwm_on(void)
{
    uint16_t p;
    for (p = 0U; p < SRM_PHASES; p++)
        EPWM_clearTripZoneFlag(EPWM_BASES[p], EPWM_TZ_INTERRUPT | EPWM_TZ_FLAG_OST);
}

/* Длительность обработчика: свободно считающий вниз таймер CPU Timer 1 (такты SYSCLK) */
uint32_t bsp_ticks(void) { return CPUTimer_getTimerCount(CPUTIMER1_BASE); }
void bsp_store_isr_ticks(uint32_t start, uint32_t stop)
{
    t_isr_us = (float)(start - stop) * 1.0e6f / (float)DEVICE_SYSCLK_FREQ;
}
float bsp_isr_time_us(void) { return t_isr_us; }
