/* include/mcp9808.h */
#ifndef MCP9808_H
#define MCP9808_H

#include <stdbool.h>
#include "FreeRTOS.h"

bool mcp9808_init(void);
bool mcp9808_read_c(float *temp_c, TickType_t bus_wait);

#endif
