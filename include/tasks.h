/* include/tasks.h */
#ifndef TASKS_H
#define TASKS_H

void control_task_start(void);
void safety_task_start(void);
void ui_task_start(void);

/* Control task reports it is alive; safety task checks this. */
void control_task_heartbeat(void);

#endif