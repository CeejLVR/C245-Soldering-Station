/* include/ads1220.h */
#ifndef ADS1220_H
#define ADS1220_H

#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"

bool ads1220_init(void);
bool ads1220_set_burnout(bool on);
bool ads1220_convert(int32_t *code, uint32_t timeout_ms);   // blocks until DRDY or timeout
float ads1220_code_to_uv(int32_t code);
void ads1220_drdy_isr(void);        // called from gpio_irq.c

#endif
