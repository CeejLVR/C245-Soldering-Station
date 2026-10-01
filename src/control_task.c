/* src/control_task.c */
#include "tasks.h"

#include "FreeRTOS.h"
#include "task.h"

#include "app_config.h"
#include "heater.h"

static volatile TickType_t s_last_beat;

void control_task_heartbeat(void) { s_last_beat = xTaskGetTickCount(); }

static void control_task(void *arg) {
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        // TODO: read temperature, run PID, call heater_set_duty()
        control_task_heartbeat();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(CONTROL_PERIOD_MS));
    }
}

void control_task_start(void) {
    xTaskCreate(control_task, "control", STACK_CONTROL, NULL, PRIO_CONTROL, NULL);
}