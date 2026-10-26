/* src/heater.c */
#include "heater.h"

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/time.h"

#include "PinDefinitions.h"
#include "app_config.h"

// heater is just a normal gpio now instead of pwm. the control task times the
// on part itself so the ADC reading always happens while the heater is off

static volatile bool s_locked_out = true;   // safety task turns this off after the watchdog test
static volatile uint64_t s_heater_off_us;   // last time GP15 went low
static volatile uint64_t s_relays_open_us;  // last time GP6 went low

void heater_init(void) {
    gpio_init(PIN_HEATER_PWM);
    gpio_put(PIN_HEATER_PWM, 0);
    gpio_set_dir(PIN_HEATER_PWM, GPIO_OUT);

    gpio_init(PIN_SENSE_RELAY);
    gpio_put(PIN_SENSE_RELAY, 0);
    gpio_set_dir(PIN_SENSE_RELAY, GPIO_OUT);
    // both relay LEDs are on this pin (~11mA) so it needs the 12mA setting
    gpio_set_drive_strength(PIN_SENSE_RELAY, GPIO_DRIVE_STRENGTH_12MA);

    uint64_t now = time_us_64();
    s_heater_off_us = now;
    s_relays_open_us = now;
}

bool heater_on(void) {
#if !CFG_HEATER_ARMED
    return false;
#else
    bool ok;
    uint32_t irq = save_and_disable_interrupts();
    uint64_t now = time_us_64();
    // only allowed if not locked out and the relays have been open long enough
    ok = !s_locked_out
      && !gpio_get_out_level(PIN_SENSE_RELAY)
      && (now - s_relays_open_us) >= RELAY_OFF_US;
    if (ok) gpio_put(PIN_HEATER_PWM, 1);
    restore_interrupts(irq);
    return ok;
#endif
}

void heater_off(void) {
    uint32_t irq = save_and_disable_interrupts();
    if (gpio_get_out_level(PIN_HEATER_PWM)) {
        gpio_put(PIN_HEATER_PWM, 0);
        s_heater_off_us = time_us_64();
    }
    restore_interrupts(irq);
}

void heater_force_off(void) {
    // safe to call from anywhere (asserts, isrs)
    gpio_put(PIN_HEATER_PWM, 0);
    gpio_put(PIN_SENSE_RELAY, 0);
    uint64_t now = time_us_64();
    s_heater_off_us = now;
    s_relays_open_us = now;
}

bool heater_is_on(void) {
    return gpio_get_out_level(PIN_HEATER_PWM);
}

void heater_set_lockout(bool locked) {
    s_locked_out = locked;
    if (locked) heater_off();
}

bool heater_is_locked_out(void) {
    return s_locked_out;
}

bool sense_relays_close(void) {
    bool ok;
    uint32_t irq = save_and_disable_interrupts();
    // heater has to be off and settled first or 24V goes into the ADC
    ok = !gpio_get_out_level(PIN_HEATER_PWM)
      && (time_us_64() - s_heater_off_us) >= HEATER_OFF_SETTLE_US;
    if (ok) gpio_put(PIN_SENSE_RELAY, 1);
    restore_interrupts(irq);
    return ok;
}

void sense_relays_open(void) {
    uint32_t irq = save_and_disable_interrupts();
    if (gpio_get_out_level(PIN_SENSE_RELAY)) {
        gpio_put(PIN_SENSE_RELAY, 0);
        s_relays_open_us = time_us_64();
    }
    restore_interrupts(irq);
}
