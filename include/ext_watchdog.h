/* include/ext_watchdog.h */
#ifndef EXT_WATCHDOG_H
#define EXT_WATCHDOG_H

#include <stdbool.h>

// 74HC123 hardware watchdog

void ext_wdt_init(void);
void ext_wdt_kick(void);
bool ext_wdt_output_high(void);     // reads 1Q on GP22

#endif
