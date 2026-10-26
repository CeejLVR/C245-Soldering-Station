/* src/safety_task.c */
#include "FreeRTOS.h"
#include "task.h"

#include "hardware/watchdog.h"
#include "pico/time.h"

#include "tasks.h"
#include "app_config.h"
#include "station.h"
#include "heater.h"
#include "ext_watchdog.h"

/*
 * Highest priority task, runs every 25ms. It checks:
 *   - control task is still running (heartbeat)
 *   - overtemp
 *   - thermal runaway (heater on a lot but the temp isnt going up)
 *   - 74HC123 output is actually high while we're kicking it
 * The 74HC123 only gets kicked if all of that is ok. if not, the hardware turns
 * the heater off by itself after ~100ms. It also feeds the pico's own watchdog
 * so if this task freezes the whole chip resets.
 */

static volatile fault_t s_latched = FAULT_NONE;
static volatile bool s_clear_req;

fault_t safety_get_fault(void) {
    return s_latched;
}

void safety_request_clear(void) {
    s_clear_req = true;
}

static inline uint32_t now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

static void wait_fed(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
    watchdog_update();
}

// kick once, output should go high. then wait and it should go low again
// heater stays locked out until this passes
static bool ext_wdt_selftest(void) {
    // the 74HC123 can give one pulse when it powers up (datasheet 11.2) so wait for that to end
    if (ext_wdt_output_high()) wait_fed(EXT_WDT_SELFTEST_MS);
    if (ext_wdt_output_high()) return false;    // stuck high

    ext_wdt_kick();
    busy_wait_us_32(50);
    if (!ext_wdt_output_high()) return false;   // didn't trigger

    wait_fed(EXT_WDT_SELFTEST_MS);
    return !ext_wdt_output_high();              // should have timed out by now
}

static void safety_task(void *arg) {
    (void)arg;
    const uint32_t boot_ms = now_ms();

    ext_wdt_init();
    watchdog_enable(INTERNAL_WDT_MS, true);     // true = pauses when debugging

    bool selftest_ok = ext_wdt_selftest();
    if (!selftest_ok) s_latched = FAULT_EXT_WDT;

    uint32_t kicks = 0;
    bool runaway_active = false;
    uint32_t runaway_start = 0;
    float runaway_t0 = 0.0f;

    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        uint32_t now = now_ms();

        if (s_clear_req) {
            s_clear_req = false;
            if (s_latched == FAULT_EXT_WDT || !selftest_ok) selftest_ok = ext_wdt_selftest();
            s_latched = selftest_ok ? FAULT_NONE : FAULT_EXT_WDT;
            kicks = 0;
            runaway_active = false;
        }

        fault_t f = FAULT_NONE;

        // give the control task a second to start up
        if (now - boot_ms > 1000 && !control_task_alive(CONTROL_HEARTBEAT_TIMEOUT_MS))
            f = FAULT_CONTROL_STALL;

        station_status_t st;
        if (station_get_status(&st) && (now - st.time_ms) < STATUS_STALE_MS) {
            if (st.tip_present && st.tip_c > TEMP_ABS_MAX_C) f = FAULT_OVERTEMP;

            // runaway: heater above RUNAWAY_DUTY and still far from setpoint,
            // temp has to go up at least RUNAWAY_MIN_RISE_C every 5s
            bool pushing = st.duty_applied >= RUNAWAY_DUTY
                        && st.tip_c < (float)st.setpoint_c - RUNAWAY_NEAR_SETPOINT_C;
            if (!pushing) {
                runaway_active = false;
            } else if (!runaway_active) {
                runaway_active = true;
                runaway_start = now;
                runaway_t0 = st.tip_c;
            } else if (now - runaway_start >= RUNAWAY_WINDOW_MS) {
                if (st.tip_c - runaway_t0 < RUNAWAY_MIN_RISE_C) f = FAULT_RUNAWAY;
                runaway_start = now;
                runaway_t0 = st.tip_c;
            }
        }

        if (selftest_ok && kicks > 2 && !ext_wdt_output_high()) f = FAULT_EXT_WDT;

        if (f != FAULT_NONE && s_latched == FAULT_NONE) {
            s_latched = f;
            heater_force_off();
        }

        if (s_latched != FAULT_NONE || !selftest_ok) {
            // stop kicking so the hardware cuts it too
            heater_set_lockout(true);
            kicks = 0;
        } else {
            ext_wdt_kick();
            if (++kicks > 2) heater_set_lockout(false);
        }

        watchdog_update();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAFETY_PERIOD_MS));
    }
}

void safety_task_start(void) {
    BaseType_t ok = xTaskCreate(safety_task, "safety", STACK_SAFETY, NULL, PRIO_SAFETY, NULL);
    configASSERT(ok == pdPASS);
}
