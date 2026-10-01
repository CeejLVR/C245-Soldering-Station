#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "heater.h"
#include "tasks.h"

int main(void) {
    stdio_init_all();
    heater_init();            // heater is forced off from start

    control_task_start();

    vTaskStartScheduler();
    for (;;) {}          
}