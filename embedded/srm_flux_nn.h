/* Модель поверхности намагничивания ВИД на основе ФИНС для встраиваемой системы. */
#ifndef SRM_FLUX_NN_H
#define SRM_FLUX_NN_H

/* Потокосцепление фазы, Вб. theta — электрический угол, рад; i — ток фазы, А. */
float srm_flux_nn(float theta, float i);

/* Производная потокосцепления по току (дифференциальная индуктивность), Гн. */
float srm_flux_nn_didi(float theta, float i);

/* Ток по потокосцеплению: метод Ньютона, n_iter итераций от начального приближения i0. */
float srm_current_from_flux(float theta, float psi, float i0, int n_iter);

/* Шаг прогнозирующего регулятора тока с непрерывным набором воздействий:
   u = (psi(theta_next, i_ref) - psi_hat)/Ts + R*i, ограничение |u| <= udc. */
float srm_mpc_voltage(float theta_next, float i_ref, float psi_hat,
                      float i, float r, float ts, float udc);
#endif
