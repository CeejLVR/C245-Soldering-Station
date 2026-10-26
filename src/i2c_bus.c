/* src/i2c_bus.c */
#include "i2c_bus.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include "semphr.h"

#include "PinDefinitions.h"
#include "app_config.h"

#define BUS        i2c0
#define TIMEOUT_US 20000

static SemaphoreHandle_t s_mutex;

void i2c_bus_init(void) {
    i2c_init(BUS, I2C_BAUD_HZ);
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
    // breakouts already have pullups, these are just backup
    gpio_pull_up(PIN_I2C_SDA);
    gpio_pull_up(PIN_I2C_SCL);
    s_mutex = xSemaphoreCreateMutex();
    configASSERT(s_mutex);
}

bool i2c_bus_lock(TickType_t wait) {
    return xSemaphoreTake(s_mutex, wait) == pdTRUE;
}

void i2c_bus_unlock(void) {
    xSemaphoreGive(s_mutex);
}

bool i2c_bus_write(uint8_t addr, const uint8_t *data, size_t len, bool nostop) {
    int r = i2c_write_timeout_us(BUS, addr, data, len, nostop, TIMEOUT_US);
    return r == (int)len;
}

bool i2c_bus_read(uint8_t addr, uint8_t *data, size_t len) {
    int r = i2c_read_timeout_us(BUS, addr, data, len, false, TIMEOUT_US);
    return r == (int)len;
}
