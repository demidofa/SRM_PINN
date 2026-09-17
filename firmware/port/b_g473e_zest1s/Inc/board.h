/*
 * Плата управления B-G473E-ZEST1S (STM32G473QET6) и четыре несимметричных моста ВИД 8/6.
 * Все сигналы выведены на разъем MC connector V2 (CN5); номера контактов указаны по UM3118 Rev 3.
 * Альтернативные функции выводов сверены с базой STM32_open_pin_data.
 *
 * ВНИМАНИЕ: силовая плата STEVAL-LVLP01 содержит три полумоста для трехфазного двигателя
 * и не подходит для ВИД 8/6. Силовой модуль с четырьмя несимметричными мостами подключается
 * к CN5 через адаптер (например, B-ZEST-ADAPT1) или собственную переходную плату.
 *
 * Фаза   Верхний ключ                  Нижний ключ
 *  A     TIM8_CH1  PC6  (CN5 A-13)      TIM1_CH1  PE9  (CN5 A-38)
 *  B     TIM8_CH2  PC7  (CN5 A-14)      TIM1_CH2  PE11 (CN5 A-39)
 *  C     TIM8_CH3  PC8  (CN5 A-15)      TIM1_CH3  PE13 (CN5 A-40)
 *  D     TIM20_CH1 PF12 (CN5 A-58)      TIM20_CH2 PF13 (CN5 A-59)
 *
 * Токи фаз, АЦП1 (группа injected): A — PC2 IN8 (A-47), B — PC3 IN9 (A-48),
 *                                    C — PA1 IN2 (B-29), D — PA2 IN3 (B-30)
 * Напряжение звена, АЦП2 (injected): PC0 IN6 (A-54)
 * Энкодер: TIM5_CH1 PF6 (B-16), TIM5_CH2 PF7 (B-17)
 * Разрешение силового модуля: PG7 (A-16, M1_Master_EN), PG6 (A-41, M2_Master_EN)
 * Светодиод LD1 — PF3 (активный низкий уровень), кнопка B2 — PC13 (активный высокий уровень)
 */
#ifndef BOARD_H
#define BOARD_H

#define PWM_FREQ_HZ        20000u
#define PWM_ARR            (170000000u / (2u * PWM_FREQ_HZ))   /* 4250, ШИМ с общим центром */

#define ENC_COUNTS         4096u      /* 1024 имп./об, режим x4 */

#define ADC_VREF           3.3f
#define ADC_FS             4095.0f
#define K_I_A_PER_V        10.0f      /* коэффициент датчика тока, А/В           */
#define K_UDC_V_PER_V      200.0f     /* коэффициент делителя напряжения звена   */

#define BTN_ACTIVE_LEVEL   GPIO_PIN_SET

#define ALIGN_DUTY         0.02f
#define ALIGN_TIME_S       1.0f
#define OFFSET_TIME_S      0.5f
#endif
