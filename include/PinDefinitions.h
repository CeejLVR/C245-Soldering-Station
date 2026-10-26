#ifndef PIN_DEFINITIONS_H
#define PIN_DEFINITIONS_H

/* All GPIO assignments */
// these are GPIO numbers not physical pins (physical pin in the comment)

// I2C0: OLED (0x3C) + MCP9808 (0x18)
#define PIN_I2C_SDA          4   // pin 6
#define PIN_I2C_SCL          5   // pin 7

// SPI0: ADS1220
#define PIN_ADC_MISO         16  // pin 21 (ADS1220 DOUT)
#define PIN_ADC_CS           17  // pin 22
#define PIN_ADC_SCLK         18  // pin 24
#define PIN_ADC_MOSI         19  // pin 25 (ADS1220 DIN)
#define PIN_ADC_DRDY         20  // pin 26

// PWM + Heater Logic
#define PIN_HEATER_PWM       15  // pin 20 (74HC08 input A)
#define PIN_WATCHDOG_KICK    21  // pin 27 (74HC123 1B)
#define PIN_WATCHDOG_STATUS  22  // pin 29 (74HC123 1Q)

// Sense relays K1 + K2 (both LEDs on one pin)
#define PIN_SENSE_RELAY      6   // pin 9

// GP7, GP8, GP9 saved for tip/stand detection later

// Rotary Encoder + Buttons
#define PIN_ENCODER_A        10  // pin 14
#define PIN_ENCODER_B        11  // pin 15
#define PIN_ENCODER_BUTTON   12  // pin 16
#define PIN_BUTTON_CON       13  // pin 17
#define PIN_BUTTON_BAK       14  // pin 19

#endif
