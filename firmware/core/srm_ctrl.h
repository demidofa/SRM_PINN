/*
 * Ядро системы управления трехфазным ВИД с несимметричными мостами.
 * Функция srm_ctrl_step вызывается в прерывании с периодом SRM_TS
 * (после завершения преобразования АЦП).
 */
#ifndef SRM_CTRL_H
#define SRM_CTRL_H
#include "srm_config.h"
#include "srm_lut.h"

typedef enum { SRM_CC_HYST = 0, SRM_CC_MPC_LUT = 1 } srm_cc_t;          /* регулятор тока      */
typedef enum { SRM_MODE_CURRENT = 0, SRM_MODE_SPEED = 1 } srm_mode_t;   /* внешний контур      */

typedef enum {
    SRM_FAULT_NONE = 0,
    SRM_FAULT_OVERCURRENT = 1,
    SRM_FAULT_OVERVOLTAGE = 2,
    SRM_FAULT_UNDERVOLTAGE = 4
} srm_fault_t;

typedef struct {                 /* измерения за текущий период */
    float i[SRM_PHASES];         /* токи фаз, А                 */
    float udc;                   /* напряжение звена, В         */
    float theta_mech;            /* механический угол, рад      */
    float omega;                 /* механическая скорость, рад/с */
} srm_meas_t;

typedef struct {                 /* воздействия на ключи */
    float duty_hi[SRM_PHASES];   /* верхний ключ фазы, 0..1 */
    float duty_lo[SRM_PHASES];   /* нижний ключ фазы, 0..1  */
    float u[SRM_PHASES];         /* заданное среднее напряжение фазы, В (для наблюдателя и отладки) */
    int   enable;                /* 0 — все ключи закрыты   */
} srm_out_t;

typedef struct {
    /* настройки */
    srm_cc_t   cc;
    srm_mode_t mode;
    float i_ref;                 /* задание тока, А (режим CURRENT)            */
    float omega_ref;             /* задание скорости, рад/с (режим SPEED)       */
    float k_adapt;               /* коэффициент коррекции таблицы; 0 — выключено */
    int   balance;               /* 1 — чередование ключей для выравнивания нагрева */
    unsigned adapt_mask;         /* фазы, по которым корректируется таблица (бит 0 — A) */
    float th_on_mot, th_off_mot, th_on_gen, th_off_gen;   /* эл. рад */
    /* состояние */
    srm_lut_t lut;
    float psi_hat[SRM_PHASES];
    float u_hold[SRM_PHASES];
    float spd_int;
    float t_ref;                 /* задание момента, Н·м */
    int   generating;
    unsigned long ticks;
    int   fault;
    /* диагностика */
    float err_sq_sum; unsigned long err_cnt;
} srm_ctrl_t;

void  srm_ctrl_init(srm_ctrl_t *c);
void  srm_ctrl_step(srm_ctrl_t *c, const srm_meas_t *m, srm_out_t *o);
void  srm_ctrl_reset_fault(srm_ctrl_t *c);
/* Среднее напряжение -> скважности верхнего и нижнего ключей (u >= 0: +U/0; u < 0: 0/-U). */
void  srm_bridge_duty(float u, float udc, int alt, float *d_hi, float *d_lo);
/* Среднее напряжение фазы при заданных скважностях (для модели объекта). */
float srm_bridge_voltage(float d_hi, float d_lo, float udc);
float srm_phase_angle(float theta_mech, int phase);
#endif
