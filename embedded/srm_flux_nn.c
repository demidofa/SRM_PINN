#include <math.h>
#include "srm_flux_nn.h"
#include "srm_flux_nn_weights.h"

/* На микроконтроллере tanh и cos можно заменить таблицами; здесь используется libm. */
static void hidden(float theta, float i, float *h)
{
    float x0 = cosf(theta);
    float x1 = i / NN_I_MAX;
    for (int k = 0; k < NN_HIDDEN; k++)
        h[k] = tanhf(NN_W[k][0] * x0 + NN_W[k][1] * x1 + NN_B[k]);
}

float srm_flux_nn(float theta, float i)
{
    float h[NN_HIDDEN], y = NN_C;
    hidden(theta, i, h);
    for (int k = 0; k < NN_HIDDEN; k++) y += NN_A[k] * h[k];
    return y;
}

float srm_flux_nn_didi(float theta, float i)
{
    float h[NN_HIDDEN], d = 0.0f;
    hidden(theta, i, h);
    for (int k = 0; k < NN_HIDDEN; k++)
        d += NN_A[k] * (1.0f - h[k] * h[k]) * NN_W[k][1];
    return d / NN_I_MAX;
}

float srm_current_from_flux(float theta, float psi, float i0, int n_iter)
{
    float i = i0 > 0.0f ? i0 : 0.0f;
    for (int n = 0; n < n_iter; n++) {
        float l = srm_flux_nn_didi(theta, i);
        if (l < 1e-4f) l = 1e-4f;              /* защита от деления на малую величину */
        i -= (srm_flux_nn(theta, i) - psi) / l;
        if (i < 0.0f) i = 0.0f;
    }
    return i;
}

float srm_mpc_voltage(float theta_next, float i_ref, float psi_hat,
                      float i, float r, float ts, float udc)
{
    float u = (srm_flux_nn(theta_next, i_ref) - psi_hat) / ts + r * i;
    if (u > udc) u = udc;
    if (u < -udc) u = -udc;
    return u;
}
