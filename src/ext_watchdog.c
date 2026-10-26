/* src/ext_watchdog.c */
#include "ext_watchdog.h"

#include "hardware/gpio.h"
#include "pico/time.h"

#include "PinDefinitions.h"

// 74HC123 with 1A tied low, so a rising edge on 1B restarts the pulse
// timeout = K * R * C = about 0.5 * 220k * 1uF = ~110ms (K is from fig 6 in the datasheet)
// kicks also have to be more than ~10.5ms apart or they dont extend it

void ext_wdt_init(void) {
    gpio_init(PIN_WATCHDOG_KICK);
    gpio_put(PIN_WATCHDOG_KICK, 0);
    gpio_set_dir(PIN_WATCHDOG_KICK, GPIO_OUT);

    gpio_init(PIN_WATCHDOG_STATUS);
    gpio_set_dir(PIN_WATCHDOG_STATUS, GPIO_IN);
    gpio_pull_down(PIN_WATCHDOG_STATUS);    // so a loose wire reads as tripped
}

void ext_wdt_kick(void) {
    gpio_put(PIN_WATCHDOG_KICK, 1);
    busy_wait_us_32(5);
    gpio_put(PIN_WATCHDOG_KICK, 0);
}

bool ext_wdt_output_high(void) {
    return gpio_get(PIN_WATCHDOG_STATUS);
}
