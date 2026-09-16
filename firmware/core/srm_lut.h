/* Табличная поверхность намагничивания с билинейной интерполяцией и онлайн-коррекцией. */
#ifndef SRM_LUT_H
#define SRM_LUT_H
#include "srm_config.h"

typedef struct {
    float psi[SRM_LUT_NTH][SRM_LUT_NI];   /* Вб; строки — угол, столбцы — ток */
    unsigned long corrections;            /* число выполненных коррекций узлов */
} srm_lut_t;

/* Исходные таблицы (формируются tools/gen_lut.py). */
extern const float SRM_LUT_INIT[SRM_LUT_NTH][SRM_LUT_NI];    /* с ошибкой -20 %, для исследования адаптации */
extern const float SRM_LUT_TRUE[SRM_LUT_NTH][SRM_LUT_NI];    /* точная таблица учебной модели             */

void  srm_lut_load(srm_lut_t *t, const float src[SRM_LUT_NTH][SRM_LUT_NI]);
float srm_lut_psi(const srm_lut_t *t, float theta_el, float i);
/* Коррекция ближайшего узла (учебник, гл. 4): psi += k*err, если d^2 < SRM_LUT_D2_MAX. */
void  srm_lut_correct(srm_lut_t *t, float theta_el, float i_ref, float err, float k);
#endif
