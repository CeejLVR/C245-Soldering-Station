#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "heater.h"
#include "gpio_irq.h"
#include "i2c_bus.h"
#include "input.h"
#include "station.h"
#include "storage.h"
#include "tasks.h"

int main(void) {
    heater_init();            // heater is forced off from start
    stdio_init_all();

    gpio_irq_init();
    i2c_bus_init();
    station_ipc_init();
    storage_init();
    input_init();

    safety_task_start();
    control_task_start();
    ui_task_start();
    log_task_start();

    vTaskStartScheduler();
    for (;;) { heater_force_off(); }    // shouldnt get here
}
