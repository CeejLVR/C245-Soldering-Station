/* include/pid.h */
#ifndef PID_H
#define PID_H

typedef struct {
    float kp, ki, kd;
    float out_min, out_max;
    float integral;
    float prev_meas;
    int primed;
} pid_ctrl_t;

void pid_init(pid_ctrl_t *p, float kp, float ki, float kd, float out_min, float out_max);
void pid_reset(pid_ctrl_t *p);
float pid_update(pid_ctrl_t *p, float setpoint, float measured, float dt_s);

#endif
