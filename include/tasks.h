/* include/tasks.h */
#ifndef TASKS_H
#define TASKS_H

#include <stdbool.h>
#include "station.h"

void control_task_start(void);
void safety_task_start(void);
void ui_task_start(void);
void log_task_start(void);

/* Control task reports it is alive; safety task checks this. */
void control_task_heartbeat(void);
bool control_task_alive(uint32_t timeout_ms);

// safety task fault, control task reads this every loop
fault_t safety_get_fault(void);
void safety_request_clear(void);

#endif
