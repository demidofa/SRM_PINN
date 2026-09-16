#include <math.h>
#include "plant.h"

#define TWO_PI 6.283185307179586

static double g(const plant_t *p, double th)
{
    double s = 0.5 * (1.0 - cos(th));
    double a = pow(s, p->p_shape), b = pow(1.0 - s, p->p_shape);
    return a / (a + b);
}

static double dg(const plant_t *p, double th)
{
    double s = 0.5 * (1.0 - cos(th));
    if (s < 1e-12) s = 1e-12;
    if (s > 1 - 1e-12) s = 1 - 1e-12;
    double a = pow(s, p->p_shape), b = pow(1.0 - s, p->p_shape);
    return p->p_shape * pow(s, p->p_shape - 1) * pow(1 - s, p->p_shape - 1) / ((a + b) * (a + b)) * 0.5 * sin(th);
}

double plant_psi(const plant_t *p, double th, double i)
{
    return p->Lu * i + p->psi_m * g(p, th) * (1.0 - exp(-p->k * i));
}

static double current(const plant_t *p, double th, double psi, double i0)
{
    double i = i0 > 0 ? i0 : 0;
    if (psi <= 0) return 0;
    double G = g(p, th);
    for (int n = 0; n < 6; n++) {
        double f = p->Lu * i + p->psi_m * G * (1.0 - exp(-p->k * i)) - psi;
        double d = p->Lu + p->psi_m * G * p->k * exp(-p->k * i);
        i -= f / d;
        if (i < 0) i = 0;
    }
    return i;
}

void plant_init(plant_t *p)
{
    p->Lu = 0.012; p->La = 0.120; p->psi_m = 1.1; p->R = 0.6; p->p_shape = 2.0;
    p->k = (p->La - p->Lu) / p->psi_m;
    p->J = 0.02; p->B = 0.002;
    p->U0 = 540.0; p->Rs = 0.05; p->Cdc = 4.7e-3;
    for (int k = 0; k < SRM_PHASES; k++) { p->psi[k] = 0; p->i[k] = 0; }
    p->theta = 0; p->omega = 0; p->udc = p->U0; p->t_load = 0; p->te = 0; p->fixed_speed = 0;
}

void plant_step(plant_t *p, const srm_out_t *o, double dt)
{
    double te = 0, p_dc = 0;
    const double stroke = TWO_PI / (SRM_PHASES * SRM_NR);
    for (int k = 0; k < SRM_PHASES; k++) {
        double v = o->enable ? srm_bridge_voltage(o->duty_hi[k], o->duty_lo[k], (float)p->udc) : -p->udc;
        if (p->i[k] <= 1e-9 && v < 0) v = 0;              /* ток равен нулю: диоды заперты */
        double th = fmod(SRM_NR * (p->theta - k * stroke), TWO_PI);
        if (th < 0) th += TWO_PI;
        p->psi[k] += (v - p->R * p->i[k]) * dt;
        if (p->psi[k] < 0) p->psi[k] = 0;
        p->i[k] = current(p, th, p->psi[k], p->i[k]);
        double i = p->i[k];
        te += SRM_NR * p->psi_m * dg(p, th) * (i - (1.0 - exp(-p->k * i)) / p->k);
        p_dc += v * i;
    }
    p->te = te;
    /* звено постоянного тока: источник через диодный выпрямитель, емкость */
    double i_src = (p->U0 - p->udc) / p->Rs;
    if (i_src < 0) i_src = 0;
    p->udc += (i_src - p_dc / p->udc) / p->Cdc * dt;
    /* механика */
    if (!p->fixed_speed)
        p->omega += (te - p->t_load - p->B * p->omega) / p->J * dt;
    p->theta = fmod(p->theta + p->omega * dt, TWO_PI);
}
