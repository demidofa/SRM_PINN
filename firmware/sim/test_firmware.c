/*
 * Проверка прошивки на ПК (software-in-the-loop): ядро srm_ctrl работает
 * с моделью объекта так же, как в прерывании микроконтроллера.
 *   1) совпадение с Python (srm_adapt.py): адаптация таблицы, 30 периодов;
 *   2) контур скорости: разгон, наброс нагрузки, торможение;
 *   3) защиты: перенапряжение звена при торможении без тормозного резистора,
 *      ложный выброс в канале тока;
 *   4) время выполнения шага регулятора на ПК.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "srm_ctrl.h"
#include "plant.h"

#define SUB 10                                    /* шагов модели на период регулятора */
static int failures = 0;
#define CHECK(cond, ...) do { int ok_ = (cond); printf("  [%s] ", ok_ ? "выполнено" : "НЕ выполнено"); \
    printf(__VA_ARGS__); printf("\n"); if (!ok_) failures++; } while (0)

static void measure(const plant_t *p, srm_meas_t *m)
{
    for (int k = 0; k < SRM_PHASES; k++) m->i[k] = (float)p->i[k];
    m->udc = (float)p->udc; m->theta_mech = (float)p->theta; m->omega = (float)p->omega;
}

/* ---------- 1. эквивалентность с Python ---------- */
static void test_equivalence(float k_adapt, const char *csv)
{
    srm_ctrl_t c; srm_out_t o; srm_meas_t m; plant_t p;
    srm_ctrl_init(&c); plant_init(&p);
    srm_lut_load(&c.lut, SRM_LUT_INIT);
    c.cc = SRM_CC_MPC_LUT; c.i_ref = 12.0f; c.k_adapt = k_adapt; c.adapt_mask = 1u;
    p.fixed_speed = 1; p.omega = 300.0 * 2 * M_PI / 60; p.Rs = 1e-6; p.Cdc = 1.0;
    const double Ts = SRM_TS, T_el = 2 * M_PI / (SRM_NR * p.omega);
    const int cycles = 30;
    long n = lround(cycles * T_el / Ts);
    double se[30] = {0}; long cn[30] = {0};
    FILE *f = fopen(csv, "w");
    fprintf(f, "period,rms_A\n");
    for (long s = 0; s < n; s++) {
        measure(&p, &m);
        m.udc = 540.0f;
        int cyc = (int)(s * Ts / T_el); if (cyc > cycles - 1) cyc = cycles - 1;
        float th = srm_phase_angle(m.theta_mech, 0);
        float on = c.th_on_mot, off = c.th_off_mot;
        float a = fmodf(th - on + 6.2831853f, 6.2831853f), b = fmodf(off - on + 6.2831853f, 6.2831853f);
        srm_ctrl_step(&c, &m, &o);
        if (a < b && m.i[0] > 0.8f * c.i_ref) { se[cyc] += pow(c.i_ref - m.i[0], 2); cn[cyc]++; }
        /* для сравнения с Python к фазе A прикладывается заданное среднее напряжение */
        for (int k = 0; k < SUB; k++) {
            srm_out_t oo = o;
            srm_bridge_duty(o.u[0], 540.0f, 0, &oo.duty_hi[0], &oo.duty_lo[0]);
            if (o.u[0] == 0.0f) { oo.duty_hi[0] = 0.0f; oo.duty_lo[0] = 1.0f; }
            plant_step(&p, &oo, Ts / SUB);
            p.udc = 540.0;
        }
    }
    for (int k = 0; k < cycles; k++) fprintf(f, "%d,%.5f\n", k + 1, sqrt(se[k] / (cn[k] ? cn[k] : 1)));
    fclose(f);
    printf("  k = %g: СКО тока в 1-м периоде %.3f А, в 30-м %.3f А, коррекций узлов %lu (файл %s)\n",
           k_adapt, sqrt(se[0] / cn[0]), sqrt(se[29] / cn[29]), c.lut.corrections, csv);
}

/* ---------- 2. контур скорости ---------- */
static void test_speed(void)
{
    srm_ctrl_t c; srm_out_t o; srm_meas_t m; plant_t p;
    srm_ctrl_init(&c); plant_init(&p);
    c.mode = SRM_MODE_SPEED; c.cc = SRM_CC_MPC_LUT; c.balance = 1;
    const double Ts = SRM_TS;
    const float w1 = 1000.0f * 2 * M_PI / 60, w2 = 500.0f * 2 * M_PI / 60;
    double wmax = 0, wmin_after_load = 1e9, udc_max = 0, t_settle = -1;
    int gen_seen = 0;
    FILE *f = fopen("speed_test_" SRM_MOTOR_TAG ".csv", "w");
    fprintf(f, "t_s,omega_ref_rpm,n_rpm,t_ref_Nm,te_Nm,udc_V,ia_A\n");
    const int ENC = 4096;                         /* импульсов энкодера на оборот */
    long cnt_prev = 0; float w_f = 0.0f;
    for (long s = 0; s < (long)(2.5 / Ts); s++) {
        double t = s * Ts;
        c.omega_ref = t < 1.5 ? w1 : w2;
        p.t_load = (t >= 0.8) ? 5.0 : 0.0;
        measure(&p, &m);
        /* энкодер: квантование угла, скорость по разности отсчетов с фильтром */
        long cnt = lround(p.theta / (2 * M_PI) * ENC);
        long d = cnt - cnt_prev; if (d < -ENC / 2) d += ENC; if (d > ENC / 2) d -= ENC;
        cnt_prev = cnt;
        w_f += 0.05f * ((float)(d * 2 * M_PI / ENC / Ts) - w_f);
        m.theta_mech = (float)(cnt * 2 * M_PI / ENC); m.omega = w_f;
        srm_ctrl_step(&c, &m, &o);
        for (int k = 0; k < SUB; k++) plant_step(&p, &o, Ts / SUB);
        if (t < 0.8) { if (p.omega > wmax) wmax = p.omega;
                       if (t_settle < 0 && fabs(p.omega - w1) < 0.02 * w1) t_settle = t; }
        if (t >= 0.8 && t < 1.5 && p.omega < wmin_after_load) wmin_after_load = p.omega;
        if (t >= 1.5 && c.generating) gen_seen = 1;
        if (p.udc > udc_max) udc_max = p.udc;
        if (s % 20 == 0)
            fprintf(f, "%.4f,%.1f,%.2f,%.3f,%.3f,%.1f,%.3f\n", t, c.omega_ref * 60 / (2 * M_PI),
                    p.omega * 60 / (2 * M_PI), c.t_ref, p.te, p.udc, p.i[0]);
    }
    fclose(f);
    double ov = (wmax - w1) / w1 * 100, dip = (w1 - wmin_after_load) / w1 * 100;
    CHECK(ov <= 10.0, "перерегулирование скорости при разгоне до 1000 об/мин: %.1f %% (норма <= 10 %%)", ov);
    CHECK(t_settle > 0, "время выхода в зону 2 %%: %.3f с", t_settle);
    CHECK(dip <= 10.0, "провал скорости при набросе 5 Н·м: %.1f %% (норма <= 10 %%)", dip);
    CHECK(fabs(p.omega - w2) < 0.02 * w2 && gen_seen, "торможение до 500 об/мин через генераторный режим: %.0f об/мин",
          p.omega * 60 / (2 * M_PI));
    printf("  максимум напряжения звена при торможении (емкость 4,7 мФ): %.1f В; файл speed_test_%s.csv\n", udc_max, SRM_MOTOR_TAG);
}

/* ---------- 3. защиты ---------- */
static void test_protection(void)
{
    srm_ctrl_t c; srm_out_t o; srm_meas_t m; plant_t p;
    srm_ctrl_init(&c); plant_init(&p);
    c.mode = SRM_MODE_SPEED;
    p.Cdc = 0.22e-3;                                  /* малая емкость, тормозного резистора нет */
    const double Ts = SRM_TS; long trip_step = -1; double udc_trip = 0;
    for (long s = 0; s < (long)(2.0 / Ts); s++) {
        double t = s * Ts;
        c.omega_ref = (float)((t < 1.0 ? 1500.0 : 0.0) * 2 * M_PI / 60);
        measure(&p, &m);
        srm_ctrl_step(&c, &m, &o);
        if (c.fault && trip_step < 0) { trip_step = s; udc_trip = p.udc; }
        for (int k = 0; k < SUB; k++) plant_step(&p, &o, Ts / SUB);
    }
    CHECK((c.fault & SRM_FAULT_OVERVOLTAGE) && trip_step * Ts > 1.0,
          "отключение по перенапряжению при торможении без резистора: t = %.3f с, Udc = %.0f В", trip_step * Ts, udc_trip);
    CHECK(o.enable == 0, "после аварии ключи закрыты");

    srm_ctrl_init(&c); plant_init(&p); c.i_ref = 10.0f; p.fixed_speed = 1; p.omega = 50;
    for (int s = 0; s < 100; s++) {
        measure(&p, &m);
        if (s == 60) m.i[1] = 40.0f;                   /* ложный выброс в канале тока фазы B */
        srm_ctrl_step(&c, &m, &o);
        if (s == 60) CHECK(o.enable == 0 && (c.fault & SRM_FAULT_OVERCURRENT),
                           "отключение по току в том же периоде регулятора");
        for (int k = 0; k < SUB; k++) plant_step(&p, &o, Ts / SUB);
    }
    srm_ctrl_reset_fault(&c);
    measure(&p, &m); srm_ctrl_step(&c, &m, &o);
    CHECK(c.fault == 0 && o.enable == 1, "сброс аварии восстанавливает работу");
}

/* ---------- 4. время выполнения ---------- */
#ifdef QEMU_TARGET
extern volatile unsigned long g_systick_wraps;
unsigned long qemu_insn_now(void);
#endif
static void test_timing(void)
{
    srm_ctrl_t c; srm_out_t o; srm_meas_t m;
    srm_ctrl_init(&c); c.k_adapt = 1e-3f; c.i_ref = 12.0f;
    m.udc = 540.0f; m.omega = 100.0f; m.i[0] = 11.0f; m.i[1] = 5.0f; m.i[2] = 0.0f;
#ifdef QEMU_TARGET
    const long N = 20000;
    unsigned long i0 = qemu_insn_now();
    for (long s = 0; s < N; s++) { m.theta_mech = (float)(s % 6283) * 1e-3f; srm_ctrl_step(&c, &m, &o); }
    double insn = (double)(qemu_insn_now() - i0) / N;
    printf("  шаг регулятора в эмуляторе Cortex-M4: %.0f машинных команд; при 170 МГц это порядка %.0f мкс "
           "(оценка, точное время измеряется DWT на плате); бюджет 100 мкс\n", insn, insn / 170e6 * 1e6 * 1.5);
#else
    const long N = 2000000;
    clock_t t0 = clock();
    for (long s = 0; s < N; s++) { m.theta_mech = (float)(s % 6283) * 1e-3f; srm_ctrl_step(&c, &m, &o); }
    double ns = 1e9 * (double)(clock() - t0) / CLOCKS_PER_SEC / N;
    printf("  шаг регулятора (%d фазы, прогнозирование, адаптация): %.0f нс на ПК; "
           "бюджет на микроконтроллере — 100 мкс\n", SRM_PHASES, ns);
#endif
    printf("  память состояния регулятора: %lu байт\n", (unsigned long)sizeof(srm_ctrl_t));
}

int main(void)
{
#ifdef QEMU_TARGET
    printf("Проверка ядра прошивки на эмулируемом Cortex-M4F (QEMU mps2-an386)\n");
    test_equivalence(1e-3f, "equiv_k1e-3_" SRM_MOTOR_TAG "_m4.csv");
    test_timing();
    return 0;
#endif
    printf("1. Совпадение с Python (srm_adapt.py)\n");
    printf("   двигатель %s: фаз %d, зубцов ротора %d\n", SRM_MOTOR_TAG, SRM_PHASES, SRM_NR);
    test_equivalence(0.0f, "equiv_k0_" SRM_MOTOR_TAG ".csv");
    test_equivalence(1e-3f, "equiv_k1e-3_" SRM_MOTOR_TAG ".csv");
    printf("2. Контур скорости\n");      test_speed();
    printf("3. Защиты\n");               test_protection();
    printf("4. Ресурсы\n");              test_timing();
    printf(failures ? "\nЕсть невыполненные проверки: %d\n" : "\nВсе проверки выполнены\n", failures);
    return failures ? 1 : 0;
}
