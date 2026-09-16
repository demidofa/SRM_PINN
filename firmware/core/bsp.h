/*
 * Интерфейс платы (board support package). Ядро управления не зависит от
 * микроконтроллера: для переноса на другую плату достаточно реализовать эти функции.
 */
#ifndef BSP_H
#define BSP_H
#include "srm_ctrl.h"

void  bsp_init(void);
void  bsp_read(srm_meas_t *m);            /* токи, напряжение звена, угол, скорость */
void  bsp_write(const srm_out_t *o);      /* скважности ключей, разрешение ШИМ     */
void  bsp_pwm_off(void);                  /* немедленное отключение всех ключей    */
float bsp_isr_time_us(void);              /* длительность последнего обработчика   */
#endif
