/* src/pid.c */
#include "pid.h"

void pid_init(pid_ctrl_t *p, float kp, float ki, float kd, float out_min, float out_max) {
    p->kp = kp;
    p->ki = ki;
    p->kd = kd;
    p->out_min = out_min;
    p->out_max = out_max;
    pid_reset(p);
}

void pid_reset(pid_ctrl_t *p) {
    p->integral = 0.0f;
    p->prev_meas = 0.0f;
    p->primed = 0;
}

float pid_update(pid_ctrl_t *p, float setpoint, float measured, float dt_s) {
    float err = setpoint - measured;

    // derivative uses the measurement instead of the error so changing the
    // setpoint doesnt cause a big spike
    float deriv = 0.0f;
    if (p->primed && dt_s > 0.0f) deriv = -(measured - p->prev_meas) / dt_s;
    p->prev_meas = measured;
    p->primed = 1;

    float p_term = p->kp * err;
    float d_term = p->kd * deriv;
    float candidate_i = p->integral + p->ki * err * dt_s;
    float out = p_term + candidate_i + d_term;

    // anti windup: if the output is maxed out, only keep the new integral if it
    // would pull the output back. otherwise it builds up the whole time the
    // iron is heating and overshoots a lot
    if (out > p->out_max) {
        out = p->out_max;
        if (err < 0.0f) p->integral = candidate_i;
    } else if (out < p->out_min) {
        out = p->out_min;
        if (err > 0.0f) p->integral = candidate_i;
    } else {
        p->integral = candidate_i;
    }
    return out;
}
