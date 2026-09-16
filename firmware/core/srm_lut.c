#include <math.h>
#include "srm_lut.h"

#define TWO_PI 6.2831853f
static const float DTH = TWO_PI / SRM_LUT_NTH;
static const float DI  = SRM_LUT_I_MAX / (SRM_LUT_NI - 1);

void srm_lut_load(srm_lut_t *t, const float src[SRM_LUT_NTH][SRM_LUT_NI])
{
    for (int a = 0; a < SRM_LUT_NTH; a++)
        for (int b = 0; b < SRM_LUT_NI; b++)
            t->psi[a][b] = src[a][b];
    t->corrections = 0;
}

static float wrap(float th)
{
    th = fmodf(th, TWO_PI);
    return th < 0.0f ? th + TWO_PI : th;
}

static void coords(float theta_el, float i, float *x, float *y)
{
    *x = wrap(theta_el) / DTH;
    if (*x >= (float)SRM_LUT_NTH) *x = 0.0f;
    if (i < 0.0f) i = 0.0f;
    if (i > SRM_LUT_I_MAX - 1e-4f) i = SRM_LUT_I_MAX - 1e-4f;
    *y = i / DI;
}

float srm_lut_psi(const srm_lut_t *t, float theta_el, float i)
{
    float x, y;
    coords(theta_el, i, &x, &y);
    int x0 = (int)x, y0 = (int)y;
    float fx = x - (float)x0, fy = y - (float)y0;
    int x1 = (x0 + 1) % SRM_LUT_NTH;
    int y1 = (y0 + 1 < SRM_LUT_NI) ? y0 + 1 : SRM_LUT_NI - 1;
    return (1 - fx) * (1 - fy) * t->psi[x0][y0] + fx * (1 - fy) * t->psi[x1][y0]
         + (1 - fx) * fy * t->psi[x0][y1] + fx * fy * t->psi[x1][y1];
}

void srm_lut_correct(srm_lut_t *t, float theta_el, float i_ref, float err, float k)
{
    float x, y;
    coords(theta_el, i_ref, &x, &y);
    float xs[2] = { floorf(x), ceilf(x) };
    float ys[2] = { floorf(y), ceilf(y) };
    for (int p = 0; p < 2; p++)
        for (int q = 0; q < 2; q++) {
            float dx = xs[p] - x, dy = ys[q] - y;
            int yn = (int)ys[q];
            if (dx * dx + dy * dy < SRM_LUT_D2_MAX && yn >= 0 && yn < SRM_LUT_NI) {
                t->psi[((int)xs[p]) % SRM_LUT_NTH][yn] += k * err;
                t->corrections++;
            }
        }
}
