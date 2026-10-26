/* src/gpio_irq.c */
#include "gpio_irq.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"

#include "PinDefinitions.h"
#include "ads1220.h"
#include "input.h"

// the sdk only lets you have one gpio irq callback so everything goes through here

static void gpio_dispatch(uint gpio, uint32_t events) {
    (void)events;
    switch (gpio) {
    case PIN_ADC_DRDY:
        ads1220_drdy_isr();
        break;
    case PIN_ENCODER_A:
    case PIN_ENCODER_B:
        input_encoder_isr();
        break;
    default:
        break;
    }
}

void gpio_irq_init(void) {
    gpio_set_irq_callback(gpio_dispatch);
    irq_set_enabled(IO_IRQ_BANK0, true);
}
