/*
 * Назначение выводов и масштабные коэффициенты: LAUNCHXL-F28379D (TMS320F28379D)
 * и силовой модуль с четырьмя несимметричными мостами для ВИД 8/6.
 * ВНИМАНИЕ: перед подключением силовой части сверить выводы со схемой платы
 * (C2000Ware, boards/launchxl_f28379d) и схемой силового модуля.
 */
#ifndef BOARD_H
#define BOARD_H

/* ШИМ: модули ePWM1..ePWM4, по одному на фазу; выход A — верхний ключ, выход B — нижний.
   GPIO0/1 — ePWM1A/B (фаза A), GPIO2/3 — ePWM2A/B (B), GPIO4/5 — ePWM3A/B (C), GPIO6/7 — ePWM4A/B (D). */
#define PWM_FREQ_HZ        20000U
#define EPWM_CLK_HZ        100000000UL                          /* SYSCLK 200 МГц / 2 */
#define PWM_TBPRD          (uint16_t)(EPWM_CLK_HZ / (2UL * PWM_FREQ_HZ))   /* 2500, пила вверх-вниз */
#define SOC_PRESCALE       2U       /* запуск АЦП каждые 2 периода ШИМ: 10 кГц */

/* Энкодер 1024 имп./об, режим x4: eQEP1A — GPIO20, eQEP1B — GPIO21 */
#define ENC_COUNTS         4096UL

/* АЦП A, 12 бит: токи фаз A..D — ADCINA0..ADCINA3, напряжение звена — ADCINA4 */
#define ADC_VREF           3.0f
#define ADC_FS             4095.0f
#define K_I_A_PER_V        10.0f      /* коэффициент датчика тока, А/В          */
#define K_UDC_V_PER_V      200.0f     /* коэффициент делителя напряжения звена  */
#define ADC_ACQ_WINDOW     15U        /* окно выборки, такты SYSCLK             */

/* Выставка ротора: постоянное напряжение на фазу A, скважность и длительность */
#define ALIGN_DUTY         0.02f
#define ALIGN_TIME_S       1.0f
#define OFFSET_TIME_S      0.5f
#endif
