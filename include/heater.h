/* include/heater.h */
#ifndef HEATER_H
#define HEATER_H

void heater_init(void);
void heater_set_duty(float duty);   // 0.0 to 1.0
void heater_force_off(void);        // register write only

#endif