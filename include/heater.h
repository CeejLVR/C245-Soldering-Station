/* include/heater.h */
#ifndef HEATER_H
#define HEATER_H

#include <stdbool.h>

// heater pin (GP15) and sense relay pin (GP6)
// relays can't close while the heater is on and the heater can't turn on
// while the relays are closed. this is only in software for now

void heater_init(void);             // call this first in main
bool heater_on(void);               // returns false if it wasn't allowed
void heater_off(void);
void heater_force_off(void);        // register write only, also opens relays
bool heater_is_on(void);

void heater_set_lockout(bool locked);
bool heater_is_locked_out(void);

bool sense_relays_close(void);      // returns false if it wasn't allowed
void sense_relays_open(void);

#endif
