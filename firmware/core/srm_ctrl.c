#include <math.h>
#include "srm_ctrl.h"

#define TWO_PI  6.2831853f
#define DEG2RAD (3.14159265f / 180.0f)

static float wrap(float th)
{
    th = fmodf(th, TWO_PI);
    return th < 0.0f ? th + TWO_PI : th;
}

float srm_phase_angle(float theta_mech, int phase)
{
    const float stroke = TWO_PI / (SRM_PHASES * SRM_NR);        /* шаг, мех. рад */
    return wrap(SRM_NR * (theta_mech - phase * stroke));
}

static int in_window(float th, float on, float off)
{
    return wrap(th - on) < wrap(off - on);
}

void srm_bridge_duty(float u, float udc, int alt, float *d_hi, float *d_lo)
{
    float r = u / udc;
    if (r > 1.0f) r = 1.0f;
    if (r < -1.0f) r = -1.0f;
    if (r >= 0.0f) {                     /* чередование +U и 0 */
        if (alt) { *d_hi = 1.0f; *d_lo = r; }      /* контур б): модулирует нижний ключ */
        else     { *d_hi = r;    *d_lo = 1.0f; }   /* контур в): модулирует верхний ключ */
    } else {                             /* чередование 0 и -U */
        if (alt) { *d_hi = 1.0f + r; *d_lo = 0.0f; }
        else     { *d_hi = 0.0f;     *d_lo = 1.0f + r; }
    }
}

float srm_bridge_voltage(float d_hi, float d_lo, float udc)
{
    /* ШИМ с общим центром: оба ключа открыты min(d) периода (+U), оба закрыты 1-max(d) (-U) */
    float both_on = d_hi < d_lo ? d_hi : d_lo;
    float both_off = 1.0f - (d_hi > d_lo ? d_hi : d_lo);
    return udc * (both_on - both_off);
}

void srm_ctrl_init(srm_ctrl_t *c)
{
    c->cc = SRM_CC_MPC_LUT;
    c->mode = SRM_MODE_CURRENT;
    c->i_ref = 0.0f; c->omega_ref = 0.0f;
    c->k_adapt = 0.0f; c->balance = 0; c->adapt_mask = 0x7u;
    c->th_on_mot = SRM_TH_ON_MOT_DEG * DEG2RAD;  c->th_off_mot = SRM_TH_OFF_MOT_DEG * DEG2RAD;
    c->th_on_gen = SRM_TH_ON_GEN_DEG * DEG2RAD;  c->th_off_gen = SRM_TH_OFF_GEN_DEG * DEG2RAD;
    srm_lut_load(&c->lut, SRM_LUT_TRUE);
    for (int p = 0; p < SRM_PHASES; p++) { c->psi_hat[p] = 0.0f; c->u_hold[p] = 0.0f; }
    c->spd_int = 0.0f; c->t_ref = 0.0f; c->generating = 0;
    c->ticks = 0; c->fault = SRM_FAULT_NONE;
    c->err_sq_sum = 0.0f; c->err_cnt = 0;
}

void srm_ctrl_reset_fault(srm_ctrl_t *c)
{
    c->fault = SRM_FAULT_NONE;
    c->spd_int = 0.0f;
    for (int p = 0; p < SRM_PHASES; p++) { c->psi_hat[p] = 0.0f; c->u_hold[p] = 0.0f; }
}

static void pwm_off(srm_out_t *o)
{
    for (int p = 0; p < SRM_PHASES; p++) { o->duty_hi[p] = 0.0f; o->duty_lo[p] = 0.0f; o->u[p] = 0.0f; }
    o->enable = 0;
}

static void speed_loop(srm_ctrl_t *c, const srm_meas_t *m)
{
    float e = c->omega_ref - m->omega;
    float t = SRM_SPD_KP * e + c->spd_int;
    /* интегрирование с ограничением (защита от насыщения интегратора) */
    if ((t < SRM_T_MAX || e < 0.0f) && (t > -SRM_T_MAX || e > 0.0f))
        c->spd_int += SRM_SPD_KI * e * SRM_TS;
    if (t > SRM_T_MAX) t = SRM_T_MAX;
    if (t < -SRM_T_MAX) t = -SRM_T_MAX;
    c->t_ref = t;
    /* гистерезис выбора режима, чтобы окна коммутации не переключались на каждом шаге */
    if (t < -0.5f) c->generating = 1;
    else if (t > 0.5f) c->generating = 0;
    float i = fabsf(t) / SRM_KT_APPROX;
    c->i_ref = i > SRM_I_MAX ? SRM_I_MAX : i;
}

void srm_ctrl_step(srm_ctrl_t *c, const srm_meas_t *m, srm_out_t *o)
{
    c->ticks++;
    /* защиты */
    for (int p = 0; p < SRM_PHASES; p++)
        if (m->i[p] > SRM_I_TRIP || m->i[p] < -2.0f) c->fault |= SRM_FAULT_OVERCURRENT;
    if (m->udc > SRM_UDC_TRIP) c->fault |= SRM_FAULT_OVERVOLTAGE;
    if (m->udc < SRM_UDC_MIN)  c->fault |= SRM_FAULT_UNDERVOLTAGE;
    if (c->fault) { pwm_off(o); return; }

    if (c->mode == SRM_MODE_SPEED) speed_loop(c, m);
    float i_ref = c->i_ref > SRM_I_MAX ? SRM_I_MAX : c->i_ref;
    float on  = c->generating ? c->th_on_gen  : c->th_on_mot;
    float off = c->generating ? c->th_off_gen : c->th_off_mot;
    float dth = SRM_NR * m->omega * SRM_TS;            /* приращение эл. угла за период */
    int alt = c->balance ? (int)((c->ticks / 50u) & 1u) : 0;

    for (int p = 0; p < SRM_PHASES; p++) {
        float th = srm_phase_angle(m->theta_mech, p);
        float i = m->i[p];
        float u;
        int active = (i_ref > 0.0f) && in_window(th, on, off);
        if (active) {
            if (c->cc == SRM_CC_HYST) {
                u = c->u_hold[p];
                if (i > i_ref + SRM_HYST_BAND) u = 0.0f;
                else if (i < i_ref - SRM_HYST_BAND) u = m->udc;
                if (u < 0.0f) u = m->udc;
            } else {
                /* прогнозирование на один шаг по таблице и наблюдателю потокосцепления */
                u = (srm_lut_psi(&c->lut, th + dth, i_ref) - c->psi_hat[p]) / SRM_TS
                    + SRM_R_PHASE * i;
                if (u > m->udc) u = m->udc;
                if (u < -m->udc) u = -m->udc;
                if (c->k_adapt > 0.0f && ((c->adapt_mask >> p) & 1u) && i > 0.5f * i_ref)
                    srm_lut_correct(&c->lut, th, i_ref, i_ref - i, c->k_adapt);
            }
            if (i > 0.8f * i_ref) { float e = i_ref - i; c->err_sq_sum += e * e; c->err_cnt++; }
        } else {
            u = (i > SRM_I_OFF) ? -m->udc : 0.0f;         /* размагничивание фазы */
        }
        c->u_hold[p] = u;
        /* наблюдатель потокосцепления: интегрирование (u - R i) */
        float ps = c->psi_hat[p] + (u - SRM_R_PHASE * i) * SRM_TS;
        c->psi_hat[p] = ps > 0.0f ? ps : 0.0f;
        if (!active && i <= SRM_I_OFF) c->psi_hat[p] = 0.0f;
        o->u[p] = u;
        srm_bridge_duty(u, m->udc, alt, &o->duty_hi[p], &o->duty_lo[p]);
        if (u == 0.0f && !active) { o->duty_hi[p] = 0.0f; o->duty_lo[p] = 0.0f; }
    }
    o->enable = 1;
}
