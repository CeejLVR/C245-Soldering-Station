/* src/heater.c */
#include "heater.h"

#include "hardware/gpio.h"
#include "hardware/pwm.h"

#include "PinDefinitions.h"

#define HEATER_PWM_WRAP 10000

void heater_init(void) {
    gpio_set_function(PIN_HEATER_GATE, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(PIN_HEATER_GATE);
    pwm_set_clkdiv(slice, 125.0f);
    pwm_set_wrap(slice, 10000);
    pwm_set_gpio_level(PIN_HEATER_GATE, 0);
    pwm_set_enabled(slice, true);
}

void heater_set_duty(float duty) {
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;
    pwm_set_gpio_level(PIN_HEATER_GATE, (uint16_t)(duty * 10000));
}

void heater_force_off(void) {
    pwm_set_gpio_level(PIN_HEATER_GATE, 0);
}