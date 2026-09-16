/* Проверка кода на ПК: совпадение с Python, время вычисления, память. */
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "srm_flux_nn.h"
#include "srm_flux_nn_weights.h"

#define N 10000
static float th[N], cur[N], psi_ref[N], dpsi_ref[N];

int main(void)
{
    FILE *f = fopen("test_vectors.csv", "r");
    if (!f) { printf("Нет test_vectors.csv, запустите export_to_c.py\n"); return 1; }
    for (int k = 0; k < N; k++) {
        double a, b, c, d;
        if (fscanf(f, "%lf,%lf,%lf,%lf", &a, &b, &c, &d) != 4) { printf("Ошибка чтения\n"); return 1; }
        th[k] = (float)a; cur[k] = (float)b; psi_ref[k] = (float)c; dpsi_ref[k] = (float)d;
    }
    fclose(f);

    double e_psi = 0, e_d = 0, e_inv = 0;
    for (int k = 0; k < N; k++) {
        double p = srm_flux_nn(th[k], cur[k]);
        e_psi = fmax(e_psi, fabs(p - psi_ref[k]));
        e_d = fmax(e_d, fabs(srm_flux_nn_didi(th[k], cur[k]) - dpsi_ref[k]));
        float i_back = srm_current_from_flux(th[k], (float)p, cur[k] * 0.8f, 6);
        if (cur[k] > 0.5f) e_inv = fmax(e_inv, fabs(i_back - cur[k]));
    }
    printf("Максимальная ошибка psi относительно Python:        %.2e Вб\n", e_psi);
    printf("Максимальная ошибка dpsi/di относительно Python:    %.2e Гн\n", e_d);
    printf("Максимальная ошибка обращения psi -> i (6 итераций): %.2e А\n", e_inv);

    int reps = 200;
    volatile float s = 0;
    clock_t t0 = clock();
    for (int r = 0; r < reps; r++)
        for (int k = 0; k < N; k++) s += srm_flux_nn(th[k], cur[k]);
    double ns = 1e9 * (double)(clock() - t0) / CLOCKS_PER_SEC / (reps * N);
    printf("Время одного вывода сети на ПК: %.1f нс\n", ns);

    int n_par = 4 * NN_HIDDEN + 1;
    printf("Память весов сети: %d байт; таблица 36x21 float32: %d байт; таблица 360x301: %d байт\n",
           n_par * 4, 36 * 21 * 4, 360 * 301 * 4);

    /* пример шага: ток 11,5 А при задании 12 А, угол 1,00 рад, следующий угол 1,0126 рад */
    float psi_hat = srm_flux_nn(1.0f, 11.5f);
    float u = srm_mpc_voltage(1.0126f, 12.0f, psi_hat, 11.5f, 0.6f, 1e-4f, 540.0f);
    printf("Пример шага регулятора: psi_hat = %.4f Вб, u = %.1f В\n", psi_hat, u);
    return (e_psi < 1e-4 && e_inv < 1e-2) ? 0 : 2;
}
