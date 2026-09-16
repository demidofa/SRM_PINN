/*
 * Назначение выводов и масштабные коэффициенты для NUCLEO-G474RE и силового модуля
 * с тремя несимметричными мостами. ВНИМАНИЕ: перед подключением силовой части сверить
 * выводы и альтернативные функции в STM32CubeMX и в схеме своего силового модуля.
 */
#ifndef BOARD_H
#define BOARD_H

/* ШИМ: 20 кГц, ШИМ с общим центром. Верхние ключи — TIM1, нижние — TIM8. */
#define PWM_FREQ_HZ        20000u
#define PWM_ARR            (170000000u / (2u * PWM_FREQ_HZ))   /* 4250 */
/* TIM1_CH1..CH3: PA8, PA9, PA10 (AF6); TIM8_CH1..CH3: PC6, PC7, PC8 (AF4) */

/* Энкодер 1024 имп./об, режим x4: TIM4_CH1 PB6, TIM4_CH2 PB7 (AF2) */
#define ENC_COUNTS         4096u

/* АЦП1, группа injected, запуск от TIM1 TRGO2 (событие обновления, каждые 2 периода ШИМ):
   ток A — PA0 (IN1), ток B — PA1 (IN2), ток C — PC0 (IN6), Udc — PC1 (IN7) */
#define ADC_VREF           3.3f
#define ADC_FS             4095.0f
#define K_I_A_PER_V        10.0f      /* коэффициент датчика тока, А/В          */
#define K_UDC_V_PER_V      200.0f     /* коэффициент делителя напряжения звена  */

/* Кнопка B1 — PC13, светодиод LD2 — PA5. Уровень нажатой кнопки проверить по схеме платы. */
#define BTN_ACTIVE_LEVEL   GPIO_PIN_SET

/* Выставка ротора: постоянное напряжение на фазу A, скважность и длительность */
#define ALIGN_DUTY         0.02f
#define ALIGN_TIME_S       1.0f
#define OFFSET_TIME_S      0.5f
#endif
