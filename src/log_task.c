/* src/log_task.c */
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "tasks.h"
#include "app_config.h"
#include "station.h"

// prints a csv line over usb serial every 200ms, for tuning the PID
// copy it from the serial monitor into excel/sheets and graph it

static void log_task(void *arg) {
    (void)arg;
    uint32_t line = 0;
    uint32_t last_seq = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(LOG_PERIOD_MS));
        station_status_t st;
        if (!station_get_status(&st) || st.seq == last_seq) continue;    // nothing new
        last_seq = st.seq;

        if (line++ % 50 == 0)
            printf("# ms,state,tip_c,cj_c,tc_uv,set_c,demand,applied,tip,armed,fault\n");
        printf("%lu,%s,%.1f,%.2f,%.1f,%d,%.3f,%.3f,%d,%d,%s\n",
               (unsigned long)st.time_ms, station_state_name(st.state),
               st.tip_c, st.cj_c, st.tc_uv, st.setpoint_c,
               st.duty_demand, st.duty_applied, st.tip_present, st.armed,
               station_fault_name(st.fault));
    }
}

void log_task_start(void) {
    BaseType_t ok = xTaskCreate(log_task, "log", STACK_LOG, NULL, PRIO_LOG, NULL);
    configASSERT(ok == pdPASS);
}
