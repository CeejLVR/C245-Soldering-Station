#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* Settings for the whole project are kept here so we only have to change them in one spot */

// 0 = heater pin never turns on (for testing sensors/ui/watchdog)
// set to 1 once the measurement timing has been checked on the scope
#define CFG_HEATER_ARMED            0

// max duty allowed. 24V/2.5ohm is ~9.6A and our fuse is 8A so keep this low for now
#define HEATER_MAX_DUTY             0.50f

/* Task priorities */
#define PRIO_SAFETY                 5
#define PRIO_CONTROL                4
#define PRIO_UI                     1
#define PRIO_LOG                    1

/* Stack sizes in WORDS */
#define STACK_SAFETY                512
#define STACK_CONTROL               768
#define STACK_UI                    768
#define STACK_LOG                   768

/* Control loop timing */
#define CONTROL_PERIOD_MS           100
#define MEAS_WINDOW_RESERVE_MS      12      // heater has to be off for at least this long every period
#define HEATER_OFF_SETTLE_US        1000    // wait after heater off before closing relays (guess, check on scope)
#define RELAY_ON_US                 2000    // AQV210E turn on is 2ms max
#define RELAY_OFF_US                1000    // AQV210E turn off is 1ms max
#define INPUT_RC_SETTLE_US          300     // R7/R8 + C8 filter
#define ADC_TIMEOUT_MS              10      // one conversion at 330 SPS is ~3.1ms

// tip detection using the ADS1220 burn out current sources
// if theres no tip the reading should go to full scale. haven't tested this yet
#define TIP_CHECK_EVERY_N           10      // every 10 cycles = once a second
#define TIP_CHECK_SETTLE_MS         8       // C8 needs time to charge from 10uA
#define TIP_CHECK_OPEN_UV           400000.0f

/* Safety */
#define SAFETY_PERIOD_MS            25      // 74HC123 needs >10.5ms between kicks, times out at ~110ms
#define EXT_WDT_SELFTEST_MS         250
#define INTERNAL_WDT_MS             500
#define CONTROL_HEARTBEAT_TIMEOUT_MS 300
#define STATUS_STALE_MS             500
#define TEMP_ABS_MAX_C              480.0f
#define TEMP_MIN_PLAUSIBLE_C        (-20.0f) // if it reads this cold the TC is probably backwards
#define CJ_MIN_C                    (-10.0f)
#define CJ_MAX_C                    85.0f
#define CJ_MAX_FAILS                20
#define ADC_MAX_FAILS               3
#define ADC_SATURATION_CODE         0x7F0000

// thermal runaway check (heater on hard but temp isnt going up, ex. broken thermocouple)
#define RUNAWAY_DUTY                0.40f
#define RUNAWAY_WINDOW_MS           5000
#define RUNAWAY_MIN_RISE_C          10.0f
#define RUNAWAY_NEAR_SETPOINT_C     15.0f   // ignore when close to setpoint (big joints)

/* Temperatures (C) */
#define SETPOINT_MIN_C              100
#define SETPOINT_MAX_C              450
#define SETPOINT_DEFAULT_C          320
#define SETPOINT_STEP_C             5
#define READY_BAND_C                5.0f
#define STANDBY_TEMP_C              150
#define SLEEP_TIMEOUT_S             600     // 10 min no input -> standby
#define AUTO_OFF_TIMEOUT_S          1800    // 30 min in standby -> off

/* Thermocouple defaults (can be changed by saved settings) */
// ~19-22 uV/K from the eevblog thread, need to calibrate
#define TC_SENS_UV_PER_C            21.0f
#define TC_OFFSET_UV                0.0f
#define TC_POLARITY                 (+1)    // change to -1 if C1 and C3 are swapped

/* PID gains, just starting values. need to tune */
#define PID_KP                      0.030f
#define PID_KI                      0.004f
#define PID_KD                      0.000f

/* Bus speeds */
#define I2C_BAUD_HZ                 400000
#define ADC_SPI_BAUD_HZ             2000000

/* Encoder + buttons */
#define ENCODER_TRANSITIONS_PER_DETENT 4    // EC11 is usually 4, try 2 if it skips
#define ENCODER_REVERSE             0
#define BUTTON_POLL_MS              5
#define BUTTON_DEBOUNCE_SAMPLES     4
#define BUTTON_LONG_PRESS_MS        800

/* UI + logging */
#define UI_FRAME_MS                 100
#define GRAPH_SAMPLE_MS             500     // 128 samples on screen = about a minute
#define LOG_PERIOD_MS               200
#define SETTINGS_SAVE_DELAY_MS      2000

#endif
