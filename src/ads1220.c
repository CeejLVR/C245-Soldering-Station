/* src/ads1220.c */
#include "ads1220.h"

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/time.h"

#include "task.h"

#include "PinDefinitions.h"
#include "app_config.h"

#define ADS_SPI        spi0

// commands (datasheet table 8-7)
#define CMD_RESET      0x06
#define CMD_START      0x08
#define CMD_POWERDOWN  0x02
#define CMD_RREG(r, n) (uint8_t)(0x20 | ((r) << 2) | ((n) - 1))
#define CMD_WREG(r, n) (uint8_t)(0x40 | ((r) << 2) | ((n) - 1))

// reg 0: AIN0-AIN1, gain 4, PGA bypassed
// has to be bypassed because the thermocouple sits at ~0V and the PGA cant go that low
#define REG0_VALUE     0x05
// reg 1: 330 SPS, normal mode, single shot. bit 0 = burn out current sources
#define REG1_BASE      0x80
#define REG1_BCS       0x01
// reg 2: internal 2.048V ref, reg 3: idacs off, use the DRDY pin
#define REG2_VALUE     0x00
#define REG3_VALUE     0x00

#define VREF_V         2.048f
#define GAIN           4.0f

static TaskHandle_t volatile s_waiter;

static inline void cs_low(void) {
    gpio_put(PIN_ADC_CS, 0);
    busy_wait_us_32(1);
}

static inline void cs_high(void) {
    busy_wait_us_32(1);
    gpio_put(PIN_ADC_CS, 1);
}

static void send_cmd(uint8_t c) {
    cs_low();
    spi_write_blocking(ADS_SPI, &c, 1);
    cs_high();
}

static void wreg(uint8_t reg, const uint8_t *vals, uint8_t n) {
    uint8_t c = CMD_WREG(reg, n);
    cs_low();
    spi_write_blocking(ADS_SPI, &c, 1);
    spi_write_blocking(ADS_SPI, vals, n);
    cs_high();
}

static void rreg(uint8_t reg, uint8_t *vals, uint8_t n) {
    uint8_t c = CMD_RREG(reg, n);
    cs_low();
    spi_write_blocking(ADS_SPI, &c, 1);
    spi_read_blocking(ADS_SPI, 0x00, vals, n);
    cs_high();
}

// DRDY went low, wake up whoever is waiting on the conversion
void ads1220_drdy_isr(void) {
    TaskHandle_t t = s_waiter;
    if (t) {
        BaseType_t woke = pdFALSE;
        vTaskNotifyGiveFromISR(t, &woke);
        portYIELD_FROM_ISR(woke);
    }
}

bool ads1220_init(void) {
    spi_init(ADS_SPI, ADC_SPI_BAUD_HZ);
    spi_set_format(ADS_SPI, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);  // SPI mode 1
    gpio_set_function(PIN_ADC_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_ADC_SCLK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_ADC_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_ADC_CS);
    gpio_put(PIN_ADC_CS, 1);
    gpio_set_dir(PIN_ADC_CS, GPIO_OUT);

    gpio_init(PIN_ADC_DRDY);
    gpio_set_dir(PIN_ADC_DRDY, GPIO_IN);
    gpio_pull_up(PIN_ADC_DRDY);

    send_cmd(CMD_RESET);
    sleep_us(100);      // needs 50us + 32 clocks after reset

    const uint8_t cfg[4] = { REG0_VALUE, REG1_BASE, REG2_VALUE, REG3_VALUE };
    wreg(0, cfg, 4);

    // read it back to make sure the chip is actually there
    uint8_t rb[4] = {0};
    rreg(0, rb, 4);
    bool ok = rb[0] == cfg[0] && rb[1] == cfg[1] && rb[2] == cfg[2] && (rb[3] & 0xFE) == cfg[3];

    gpio_set_irq_enabled(PIN_ADC_DRDY, GPIO_IRQ_EDGE_FALL, true);
    return ok;
}

bool ads1220_set_burnout(bool on) {
    uint8_t v = REG1_BASE | (on ? REG1_BCS : 0);
    wreg(1, &v, 1);     // writing a register also starts a conversion
    uint8_t rb = 0;
    rreg(1, &rb, 1);
    return rb == v;
}

static void read_raw(uint8_t b[3]) {
    cs_low();
    spi_read_blocking(ADS_SPI, 0x00, b, 3);
    cs_high();
}

bool ads1220_convert(int32_t *code, uint32_t timeout_ms) {
    uint8_t b[3];
    // if theres an old result that never got read DRDY is still low and we'd
    // miss the edge, so read it out first
    if (gpio_get(PIN_ADC_DRDY) == 0) read_raw(b);

    s_waiter = xTaskGetCurrentTaskHandle();
    (void)ulTaskNotifyTake(pdTRUE, 0);  // clear old notifications

    send_cmd(CMD_START);
    bool ready = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeout_ms)) > 0;
    s_waiter = NULL;
    if (!ready && gpio_get(PIN_ADC_DRDY) == 0) ready = true;   // in case the irq got missed
    if (!ready) return false;

    read_raw(b);

    // 24 bit twos complement -> int32
    int32_t v = ((int32_t)b[0] << 16) | ((int32_t)b[1] << 8) | b[2];
    if (v & 0x800000) v -= 0x1000000;
    *code = v;
    return true;
}

float ads1220_code_to_uv(int32_t code) {
    // 1 LSB = (2 * 2.048 / 4) / 2^24, about 61nV
    return (float)code * (VREF_V / GAIN) / 8388608.0f * 1e6f;
}
