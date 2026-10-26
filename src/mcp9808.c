/* src/mcp9808.c */
#include "mcp9808.h"

#include "i2c_bus.h"

#define ADDR        0x18    // A0-A2 tied to GND
#define REG_TA      0x05    // ambient temp
#define REG_MFG_ID  0x06    // should read 0x0054
#define REG_DEV_ID  0x07    // upper byte should be 0x04

static bool read_reg16(uint8_t reg, uint16_t *out) {
    uint8_t b[2];
    if (!i2c_bus_write(ADDR, &reg, 1, true)) return false;
    if (!i2c_bus_read(ADDR, b, 2)) return false;
    *out = ((uint16_t)b[0] << 8) | b[1];
    return true;
}

bool mcp9808_init(void) {
    uint16_t mfg = 0, dev = 0;
    if (!i2c_bus_lock(pdMS_TO_TICKS(100))) return false;
    bool ok = read_reg16(REG_MFG_ID, &mfg) && read_reg16(REG_DEV_ID, &dev);
    i2c_bus_unlock();
    return ok && mfg == 0x0054 && (dev >> 8) == 0x04;
}

bool mcp9808_read_c(float *temp_c, TickType_t bus_wait) {
    uint16_t raw;
    if (!i2c_bus_lock(bus_wait)) return false;
    bool ok = read_reg16(REG_TA, &raw);
    i2c_bus_unlock();
    if (!ok) return false;

    // top 3 bits are alert flags, bit 12 is the sign, 0.0625 C per bit
    raw &= 0x1FFF;
    int16_t v = (int16_t)(raw & 0x0FFF);
    if (raw & 0x1000) v -= 4096;
    *temp_c = (float)v * 0.0625f;
    return true;
}
