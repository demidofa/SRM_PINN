/* Реализация интерфейса платы bsp.h для STM32G474 (HAL). */
#include <math.h>
#include "stm32g4xx_hal.h"
#include "board.h"
#include "bsp.h"

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim1, htim8, htim4;

static float adc_off[3] = { 2048.0f, 2048.0f, 2048.0f };
static float theta_off = 0.0f;           /* механический угол, соответствующий выставке */
static int32_t enc_prev = 0;
static float omega_f = 0.0f;
static float t_isr_us = 0.0f;
static uint16_t raw[4];

void bsp_adc_store(uint16_t ia, uint16_t ib, uint16_t ic, uint16_t udc)
{
    raw[0] = ia; raw[1] = ib; raw[2] = ic; raw[3] = udc;
}

void bsp_offset_accumulate(int reset, int n)
{
    static float acc[3];
    if (reset) { acc[0] = acc[1] = acc[2] = 0.0f; return; }
    for (int k = 0; k < 3; k++) acc[k] += raw[k];
    if (n > 0) for (int k = 0; k < 3; k++) adc_off[k] = acc[k] / (float)n;
}

void bsp_set_theta_offset(void)
{
    /* после выставки ротор стоит в согласованном положении фазы A: электрический угол pi */
    float th = (float)__HAL_TIM_GET_COUNTER(&htim4) * 6.2831853f / ENC_COUNTS;
    theta_off = th - 3.14159265f / 4.0f;          /* pi эл. рад = pi/Nr мех. рад, Nr = 4 */
    enc_prev = (int32_t)__HAL_TIM_GET_COUNTER(&htim4);
    omega_f = 0.0f;
}

void bsp_init(void) { }

void bsp_read(srm_meas_t *m)
{
    const float k = ADC_VREF / ADC_FS;
    for (int p = 0; p < 3; p++) m->i[p] = ((float)raw[p] - adc_off[p]) * k * K_I_A_PER_V;
    m->udc = (float)raw[3] * k * K_UDC_V_PER_V;
    int32_t cnt = (int32_t)__HAL_TIM_GET_COUNTER(&htim4);
    int32_t d = cnt - enc_prev;
    if (d >  (int32_t)(ENC_COUNTS / 2)) d -= (int32_t)ENC_COUNTS;
    if (d < -(int32_t)(ENC_COUNTS / 2)) d += (int32_t)ENC_COUNTS;
    enc_prev = cnt;
    omega_f += 0.05f * ((float)d * 6.2831853f / ENC_COUNTS / SRM_TS - omega_f);
    float th = (float)cnt * 6.2831853f / ENC_COUNTS - theta_off;
    th = fmodf(th, 6.2831853f);
    m->theta_mech = th < 0.0f ? th + 6.2831853f : th;
    m->omega = omega_f;
}

static void set_ccr(TIM_HandleTypeDef *h, uint32_t ch, float d)
{
    if (d < 0.0f) d = 0.0f;
    if (d > 1.0f) d = 1.0f;
    __HAL_TIM_SET_COMPARE(h, ch, (uint32_t)(d * (float)PWM_ARR));
}

void bsp_write(const srm_out_t *o)
{
    static const uint32_t ch[3] = { TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3 };
    if (!o->enable) { bsp_pwm_off(); return; }
    for (int p = 0; p < 3; p++) {
        set_ccr(&htim1, ch[p], o->duty_hi[p]);
        set_ccr(&htim8, ch[p], o->duty_lo[p]);
    }
    __HAL_TIM_MOE_ENABLE(&htim1);
    __HAL_TIM_MOE_ENABLE(&htim8);
}

void bsp_pwm_off(void)
{
    for (uint32_t ch = TIM_CHANNEL_1; ch <= TIM_CHANNEL_3; ch += 4u) {
        __HAL_TIM_SET_COMPARE(&htim1, ch, 0u);
        __HAL_TIM_SET_COMPARE(&htim8, ch, 0u);
    }
    htim1.Instance->BDTR &= ~TIM_BDTR_MOE;       /* выходы в неактивное состояние */
    htim8.Instance->BDTR &= ~TIM_BDTR_MOE;
}

/* Измерение длительности обработчика счетчиком тактов ядра (DWT) */
void bsp_timer_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
uint32_t bsp_cycles(void) { return DWT->CYCCNT; }
void bsp_store_isr_cycles(uint32_t c) { t_isr_us = (float)c * 1.0e6f / (float)SystemCoreClock; }
float bsp_isr_time_us(void) { return t_isr_us; }
