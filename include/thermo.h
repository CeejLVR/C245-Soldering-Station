/* include/thermo.h */
#ifndef THERMO_H
#define THERMO_H

typedef struct {
    float sens_uv_per_c;    // uV per degree C
    float offset_uv;
    int polarity;           // +1 or -1
} tc_cal_t;

float thermo_tip_c(float tc_uv, float cj_c, const tc_cal_t *cal);

#endif
