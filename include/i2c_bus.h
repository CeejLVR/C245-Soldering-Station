/* include/i2c_bus.h */
#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "FreeRTOS.h"

// OLED and MCP9808 are on the same bus so there's a mutex around it

void i2c_bus_init(void);
bool i2c_bus_lock(TickType_t wait);
void i2c_bus_unlock(void);

// need to have the lock before calling these
bool i2c_bus_write(uint8_t addr, const uint8_t *data, size_t len, bool nostop);
bool i2c_bus_read(uint8_t addr, uint8_t *data, size_t len);

#endif
