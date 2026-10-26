/* include/input.h */
#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include "FreeRTOS.h"

typedef enum {
    EV_ENC_CW,
    EV_ENC_CCW,
    EV_PSH, EV_PSH_LONG,    // encoder button
    EV_CON, EV_CON_LONG,
    EV_BAK, EV_BAK_LONG,
} input_event_t;

void input_init(void);      // call before the scheduler starts
bool input_get_event(input_event_t *ev, TickType_t wait);
void input_encoder_isr(void);

#endif
