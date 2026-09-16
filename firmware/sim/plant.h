/* Модель объекта для проверки прошивки на ПК: ВИД 6/4, звено постоянного тока, механика. */
#ifndef PLANT_H
#define PLANT_H
#include "srm_ctrl.h"

typedef struct {
    double Lu, La, psi_m, k, R, p_shape;   /* электромагнитные параметры (как в srm_model.py) */
    double J, B;                           /* момент инерции, кг·м², вязкое трение, Н·м·с     */
    double U0, Rs, Cdc;                    /* источник через выпрямитель и емкость звена       */
    double psi[SRM_PHASES], i[SRM_PHASES];
    double theta, omega, udc, t_load, te;
    int    fixed_speed;                    /* 1 — скорость задана, механика не интегрируется  */
} plant_t;

void   plant_init(plant_t *p);
void   plant_step(plant_t *p, const srm_out_t *o, double dt);
double plant_psi(const plant_t *p, double th, double i);
#endif
