/*
 * Параметры системы управления вентильно-индукторным приводом (учебная модель 6/4).
 * Значения согласованы с моделью scripts/srm_model.py. Для реального двигателя
 * параметры и таблица поверхности намагничивания заменяются измеренными.
 */
#ifndef SRM_CONFIG_H
#define SRM_CONFIG_H

/* Выбор двигателя: по умолчанию учебная модель 6/4; для стенда 8/6 задать SRM_MOTOR_8_6 */
#if defined(SRM_MOTOR_8_6)
#define SRM_PHASES          4          /* четырехфазный ВИД 8/6 (лабораторный стенд)    */
#define SRM_NR              6          /* число зубцов ротора                           */
#define SRM_MOTOR_TAG       "8_6"
#else
#define SRM_PHASES          3          /* трехфазный ВИД 6/4 (учебная модель)           */
#define SRM_NR              4
#define SRM_MOTOR_TAG       "6_4"
#endif
#define SRM_TS              1.0e-4f    /* период работы регулятора, с (10 кГц)          */
#define SRM_R_PHASE         0.6f       /* сопротивление фазы, Ом                        */
#define SRM_I_MAX           20.0f      /* ограничение задания тока, А                   */
#define SRM_I_TRIP          26.0f      /* уставка аварийного отключения по току, А      */
#define SRM_UDC_TRIP        650.0f     /* уставка отключения по напряжению звена, В     */
#define SRM_UDC_MIN         200.0f     /* минимальное напряжение звена для работы, В    */
#define SRM_I_OFF           1.0e-3f    /* ток, ниже которого фаза считается обесточенной */

#define SRM_TH_ON_MOT_DEG   20.0f      /* углы коммутации, эл. град                     */
#define SRM_TH_OFF_MOT_DEG  150.0f
#define SRM_TH_ON_GEN_DEG   150.0f
#define SRM_TH_OFF_GEN_DEG  290.0f

#define SRM_HYST_BAND       0.8f       /* полуширина петли релейного регулятора, А      */

/* Таблица поверхности намагничивания: электрический угол 0..2pi, ток 0..SRM_LUT_I_MAX */
#define SRM_LUT_NTH         36
#define SRM_LUT_NI          21
#define SRM_LUT_I_MAX       30.0f
#define SRM_LUT_D2_MAX      0.25f      /* порог нормированного квадрата расстояния      */

/* Регулятор скорости */
#define SRM_SPD_KP          0.8f       /* Н·м/(рад/с)                                   */
#define SRM_SPD_KI          8.0f       /* Н·м/рад                                      */
#define SRM_T_MAX           20.0f      /* ограничение задания момента, Н·м              */
#define SRM_KT_APPROX       1.0f       /* приближенный коэффициент момента, Н·м/А       */
#endif
