/* src/control_task.c */
#include <math.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include "hardware/watchdog.h"
#include "pico/time.h"

#include "tasks.h"
#include "app_config.h"
#include "station.h"
#include "heater.h"
#include "ads1220.h"
#include "mcp9808.h"
#include "thermo.h"
#include "pid.h"
#include "storage.h"

/*
 * Every 100ms:
 *   1. heater off, wait for it to settle
 *   2. read the MCP9808 (cold junction)
 *   3. close the relays, wait 2ms, do one ADS1220 conversion, open the relays
 *   4. run the PID
 *   5. heater on for (duty * 100ms) then off again before the next cycle
 *
 * heater.c won't let the relays close while the heater is on, so if this order
 * ever gets messed up it shows a FAULT_INTERLOCK instead of frying the ADC
 */

static volatile TickType_t s_last_beat;
static TimerHandle_t s_sleep_timer, s_off_timer;

void control_task_heartbeat(void) {
    s_last_beat = xTaskGetTickCount();
}

bool control_task_alive(uint32_t timeout_ms) {
    return (xTaskGetTickCount() - s_last_beat) <= pdMS_TO_TICKS(timeout_ms);
}

// vTaskDelay only works in 1ms ticks, so sleep for most of it and spin for the rest
static void wait_at_least_us(uint32_t us) {
    uint64_t end = time_us_64() + us;
    if (us >= 2000) vTaskDelay(pdMS_TO_TICKS(us / 1000 - 1));
    while (time_us_64() < end) tight_loop_contents();
}

static void sleep_timer_cb(TimerHandle_t t) { (void)t; station_send_cmd(CMD_STANDBY, 0); }
static void off_timer_cb(TimerHandle_t t) { (void)t; station_send_cmd(CMD_SET_ENABLE, 0); }

static inline uint32_t now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

// keep the first fault that happened
static void set_fault(fault_t *fault, fault_t f) {
    if (*fault == FAULT_NONE) *fault = f;
}

static void control_task(void *arg) {
    (void)arg;

    fault_t fault = FAULT_NONE;
    if (watchdog_caused_reboot()) set_fault(&fault, FAULT_WDT_RESET);
    if (!ads1220_init()) set_fault(&fault, FAULT_ADC);
    if (!mcp9808_init()) set_fault(&fault, FAULT_CJ_SENSOR);

    s_sleep_timer = xTimerCreate("sleep", pdMS_TO_TICKS(SLEEP_TIMEOUT_S * 1000u), pdFALSE, NULL, sleep_timer_cb);
    s_off_timer = xTimerCreate("autooff", pdMS_TO_TICKS(AUTO_OFF_TIMEOUT_S * 1000u), pdFALSE, NULL, off_timer_cb);
    configASSERT(s_sleep_timer && s_off_timer);

    pid_ctrl_t pid;
    pid_init(&pid, PID_KP, PID_KI, PID_KD, 0.0f, HEATER_MAX_DUTY);

    settings_t cfg;
    station_status_t st = {0};
    bool enabled = false, standby = false, tip_present = false;
    float cj = 25.0f, tip = 25.0f, uv = 0.0f;
    int cj_fails = 0, adc_fails = 0;
    uint32_t cycle = 0, seq = 0;

    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        TickType_t t0 = xTaskGetTickCount();
        heater_off();
        storage_get(&cfg);

        // handle commands from the ui and the timers
        station_cmd_t c;
        while (station_receive_cmd(&c)) {
            switch (c.type) {
            case CMD_TOGGLE_ENABLE:
            case CMD_SET_ENABLE:
                enabled = (c.type == CMD_TOGGLE_ENABLE) ? !enabled : (c.value != 0);
                standby = false;
                xTimerStop(s_off_timer, 0);
                if (enabled) xTimerReset(s_sleep_timer, 0);
                else xTimerStop(s_sleep_timer, 0);
                pid_reset(&pid);
                break;
            case CMD_WAKE:
                if (standby) { standby = false; xTimerStop(s_off_timer, 0); }
                if (enabled) xTimerReset(s_sleep_timer, 0);
                break;
            case CMD_STANDBY:
                if (enabled && !standby) { standby = true; xTimerReset(s_off_timer, 0); }
                break;
            case CMD_CLEAR_FAULT:
                fault = FAULT_NONE;
                cj_fails = adc_fails = 0;
                enabled = false;    // make the user turn it back on
                safety_request_clear();
                pid_reset(&pid);
                break;
            }
        }

        wait_at_least_us(HEATER_OFF_SETTLE_US);

        // cold junction. if the bus is busy just use the last value
        float cj_new;
        if (mcp9808_read_c(&cj_new, pdMS_TO_TICKS(5)) && cj_new > CJ_MIN_C && cj_new < CJ_MAX_C) {
            cj = cj_new;
            cj_fails = 0;
        } else if (++cj_fails >= CJ_MAX_FAILS) {
            set_fault(&fault, FAULT_CJ_SENSOR);
        }

        // measurement window (heater is off here)
        bool meas_ok = false;
        int32_t code = 0;
        if (sense_relays_close()) {
            wait_at_least_us(RELAY_ON_US + INPUT_RC_SETTLE_US);

            // tip check once a second
            if (cycle % TIP_CHECK_EVERY_N == 0) {
                int32_t bcs_code;
                if (ads1220_set_burnout(true)) {
                    vTaskDelay(pdMS_TO_TICKS(TIP_CHECK_SETTLE_MS));
                    if (ads1220_convert(&bcs_code, ADC_TIMEOUT_MS))
                        tip_present = ads1220_code_to_uv(bcs_code) < TIP_CHECK_OPEN_UV;
                }
                ads1220_set_burnout(false);
                wait_at_least_us(1000);     // let C8 discharge before the real reading
            }

            if (ads1220_convert(&code, ADC_TIMEOUT_MS)) {
                meas_ok = true;
                adc_fails = 0;
            } else if (++adc_fails >= ADC_MAX_FAILS) {
                set_fault(&fault, FAULT_ADC);
            }
            sense_relays_open();
            wait_at_least_us(RELAY_OFF_US);     // relays need 1ms to fully open
        } else {
            set_fault(&fault, FAULT_INTERLOCK);
        }

        if (meas_ok) {
            uv = ads1220_code_to_uv(code);
            tc_cal_t cal = { cfg.tc_sens_uv_per_c, cfg.tc_offset_uv, cfg.tc_polarity };
            tip = thermo_tip_c(uv, cj, &cal);
            if (tip_present) {
                if (abs(code) >= ADC_SATURATION_CODE) set_fault(&fault, FAULT_TC_RANGE);
                else if (tip > TEMP_ABS_MAX_C) set_fault(&fault, FAULT_OVERTEMP);
                else if (tip < TEMP_MIN_PLAUSIBLE_C) set_fault(&fault, FAULT_TC_RANGE);
            }
        }
        // dont copy the safety fault into our own variable, otherwise clearing it
        // in the safety task never shows up here
        fault_t shown = (fault != FAULT_NONE) ? fault : safety_get_fault();

        // state machine
        int target = standby ? (cfg.standby_c < cfg.setpoint_c ? cfg.standby_c : cfg.setpoint_c)
                             : cfg.setpoint_c;
        station_state_t state;
        if (shown != FAULT_NONE) state = ST_FAULT;
        else if (!enabled) state = ST_OFF;
        else if (!tip_present) state = ST_NO_TIP;
        else if (standby) state = ST_STANDBY;
        else if (fabsf(tip - (float)target) <= READY_BAND_C) state = ST_READY;
        else state = ST_HEATING;

        float demand = 0.0f;
        bool regulate = meas_ok && (state == ST_HEATING || state == ST_READY || state == ST_STANDBY);
        if (regulate) demand = pid_update(&pid, (float)target, tip, CONTROL_PERIOD_MS / 1000.0f);
        else pid_reset(&pid);

        // figure out how long the heater can be on without running into the next measurement
        TickType_t period = pdMS_TO_TICKS(CONTROL_PERIOD_MS);
        TickType_t elapsed = xTaskGetTickCount() - t0;
        TickType_t slot = period - pdMS_TO_TICKS(MEAS_WINDOW_RESERVE_MS);
        if (elapsed + slot + 1 > period) slot = (elapsed + 1 < period) ? period - elapsed - 1 : 0;
        TickType_t on = (TickType_t)lroundf(demand * (float)period);
        if (on > slot) on = slot;

        bool heating = on > 0 && heater_on();
        // if it should have been allowed but wasn't, something is wrong with the sequence
        if (on > 0 && !heating && CFG_HEATER_ARMED && !heater_is_locked_out())
            set_fault(&fault, FAULT_INTERLOCK);

        st.state = state;
        st.fault = shown;
        st.tip_c = tip;
        st.cj_c = cj;
        st.tc_uv = uv;
        st.duty_demand = demand;
        st.duty_applied = heating ? (float)on / (float)period : 0.0f;
        st.setpoint_c = (int16_t)target;
        st.preset = cfg.active_preset;
        st.tip_present = tip_present;
        st.enabled = enabled;
        st.armed = CFG_HEATER_ARMED && !heater_is_locked_out();
        st.time_ms = now_ms();
        st.seq = ++seq;
        station_publish(&st);
        control_task_heartbeat();

        if (heating) {
            vTaskDelay(on);
            heater_off();
        }

        cycle++;
        vTaskDelayUntil(&last_wake, period);
    }
}

void control_task_start(void) {
    BaseType_t ok = xTaskCreate(control_task, "control", STACK_CONTROL, NULL, PRIO_CONTROL, NULL);
    configASSERT(ok == pdPASS);
}
