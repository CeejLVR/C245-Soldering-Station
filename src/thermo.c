/* src/thermo.c */
#include "thermo.h"

// tip temp = cold junction temp + polarity * (V_tc - offset) / sensitivity
// TODO: maybe switch to a calibration table later. can get points from solder
// melting temps (63/37 is 183C, SAC305 is around 217-220C)

float thermo_tip_c(float tc_uv, float cj_c, const tc_cal_t *cal) {
    float s = cal->sens_uv_per_c > 1.0f ? cal->sens_uv_per_c : 1.0f;    // dont divide by 0
    return cj_c + (float)cal->polarity * (tc_uv - cal->offset_uv) / s;
}
