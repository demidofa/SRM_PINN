/* Реализация интерфейса платы bsp.h для B-G473E-ZEST1S (HAL), ВИД 8/6. */
#include <math.h>
#include "stm32g4xx_hal.h"
#include "board.h"
#include "bsp.h"

extern TIM_HandleTypeDef htim1, htim8, htim20, htim5;

typedef struct { TIM_HandleTypeDef *h; uint32_t ch; } pwm_out_t;
/* верхний и нижний ключи фаз A..D */
static const pwm_out_t HI[SRM_PHASES] = {
    { &htim8, TIM_CHANNEL_1 }, { &htim8, TIM_CHANNEL_2 }, { &htim8, TIM_CHANNEL_3 }, { &htim20, TIM_CHANNEL_1 } };
static const pwm_out_t LO[SRM_PHASES] = {
    { &htim1, TIM_CHANNEL_1 }, { &htim1, TIM_CHANNEL_2 }, { &htim1, TIM_CHANNEL_3 }, { &htim20, TIM_CHANNEL_2 } };

static float adc_off[SRM_PHASES] = { 2048.0f, 2048.0f, 2048.0f, 2048.0f };
static float acc[SRM_PHASES];
static uint16_t raw_i[SRM_PHASES], raw_u;
static float theta_off = 0.0f;
static int32_t enc_prev = 0;
static float omega_f = 0.0f;
static float t_isr_us = 0.0f;

void bsp_adc_store(const uint16_t *i, uint16_t udc)
{
    for (int k = 0; k < SRM_PHASES; k++) raw_i[k] = i[k];
    raw_u = udc;
}

void bsp_offset_accumulate(int reset, int n)
{
    if (reset) { for (int k = 0; k < SRM_PHASES; k++) acc[k] = 0.0f; return; }
    for (int k = 0; k < SRM_PHASES; k++) acc[k] += raw_i[k];
    if (n > 0) for (int k = 0; k < SRM_PHASES; k++) adc_off[k] = acc[k] / (float)n;
}

void bsp_set_theta_offset(void)
{
    /* ротор выставлен в согласованное положение фазы A: электрический угол pi = pi/Nr мех. рад */
    float th = (float)__HAL_TIM_GET_COUNTER(&htim5) * 6.2831853f / ENC_COUNTS;
    theta_off = th - 3.14159265f / (float)SRM_NR;
    enc_prev = (int32_t)__HAL_TIM_GET_COUNTER(&htim5);
    omega_f = 0.0f;
}

void bsp_init(void) { }

void bsp_read(srm_meas_t *m)
{
    const float k = ADC_VREF / ADC_FS;
    for (int p = 0; p < SRM_PHASES; p++) m->i[p] = ((float)raw_i[p] - adc_off[p]) * k * K_I_A_PER_V;
    m->udc = (float)raw_u * k * K_UDC_V_PER_V;
    int32_t cnt = (int32_t)__HAL_TIM_GET_COUNTER(&htim5);
    int32_t d = cnt - enc_prev;
    if (d >  (int32_t)(ENC_COUNTS / 2)) d -= (int32_t)ENC_COUNTS;
    if (d < -(int32_t)(ENC_COUNTS / 2)) d += (int32_t)ENC_COUNTS;
    enc_prev = cnt;
    omega_f += 0.05f * ((float)d * 6.2831853f / ENC_COUNTS / SRM_TS - omega_f);
    float th = fmodf((float)cnt * 6.2831853f / ENC_COUNTS - theta_off, 6.2831853f);
    m->theta_mech = th < 0.0f ? th + 6.2831853f : th;
    m->omega = omega_f;
}

static void set_ccr(const pwm_out_t *o, float d)
{
    if (d < 0.0f) d = 0.0f;
    if (d > 1.0f) d = 1.0f;
    __HAL_TIM_SET_COMPARE(o->h, o->ch, (uint32_t)(d * (float)PWM_ARR));
}

static void moe(int on)
{
    TIM_HandleTypeDef *t[3] = { &htim1, &htim8, &htim20 };
    for (int k = 0; k < 3; k++) {
        if (on) t[k]->Instance->BDTR |= TIM_BDTR_MOE;
        else    t[k]->Instance->BDTR &= ~TIM_BDTR_MOE;
    }
}

void bsp_write(const srm_out_t *o)
{
    if (!o->enable) { bsp_pwm_off(); return; }
    for (int p = 0; p < SRM_PHASES; p++) {
        set_ccr(&HI[p], o->duty_hi[p]);
        set_ccr(&LO[p], o->duty_lo[p]);
    }
    moe(1);
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
}

void bsp_pwm_off(void)
{
    for (int p = 0; p < SRM_PHASES; p++) { set_ccr(&HI[p], 0.0f); set_ccr(&LO[p], 0.0f); }
    moe(0);                                           /* выходы в неактивное состояние */
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);
}

void bsp_timer_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
uint32_t bsp_cycles(void) { return DWT->CYCCNT; }
void bsp_store_isr_cycles(uint32_t c) { t_isr_us = (float)c * 1.0e6f / (float)SystemCoreClock; }
float bsp_isr_time_us(void) { return t_isr_us; }
