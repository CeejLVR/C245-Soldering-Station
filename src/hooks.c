/* src/hooks.c */
#include "hooks.h"

#include "FreeRTOS.h"
#include "task.h"

#include "heater.h"

void vAssertCalled(const char *file, int line) {
    (void)file; (void)line;
    taskDISABLE_INTERRUPTS();
    heater_force_off();
    for (;;) {}                       // watchdog resets the chip
}

void vApplicationMallocFailedHook(void) {
    heater_force_off();
    configASSERT(0);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
    (void)task; (void)name;
    heater_force_off();
    configASSERT(0);
}