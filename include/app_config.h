#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* Task priorities */
#define PRIO_UI          1
#define PRIO_LOG         1
#define PRIO_CONTROL     4
#define PRIO_SAFETY      5

/* Stack sizes in WORDS */
#define STACK_UI         512
#define STACK_LOG        384
#define STACK_CONTROL    384
#define STACK_SAFETY     256

/* Loop periods */
#define CONTROL_PERIOD_MS   50
#define SAFETY_PERIOD_MS    100

#endif