/* src/station.c */
#include "station.h"

#include "FreeRTOS.h"
#include "queue.h"

static QueueHandle_t s_status_mailbox;  // length 1, always has the latest status
static QueueHandle_t s_cmd_queue;

void station_ipc_init(void) {
    s_status_mailbox = xQueueCreate(1, sizeof(station_status_t));
    s_cmd_queue = xQueueCreate(8, sizeof(station_cmd_t));
    configASSERT(s_status_mailbox && s_cmd_queue);
}

void station_publish(const station_status_t *st) {
    xQueueOverwrite(s_status_mailbox, st);
}

bool station_get_status(station_status_t *out) {
    return xQueuePeek(s_status_mailbox, out, 0) == pdTRUE;
}

bool station_send_cmd(station_cmd_type_t type, int32_t value) {
    station_cmd_t c = { type, value };
    return xQueueSend(s_cmd_queue, &c, 0) == pdTRUE;
}

bool station_receive_cmd(station_cmd_t *out) {
    return xQueueReceive(s_cmd_queue, out, 0) == pdTRUE;
}

const char *station_state_name(station_state_t s) {
    switch (s) {
    case ST_OFF:     return "OFF";
    case ST_HEATING: return "HEATING";
    case ST_READY:   return "READY";
    case ST_STANDBY: return "STANDBY";
    case ST_NO_TIP:  return "NO TIP";
    case ST_FAULT:   return "FAULT";
    }
    return "?";
}

const char *station_fault_name(fault_t f) {
    switch (f) {
    case FAULT_NONE:          return "none";
    case FAULT_ADC:           return "ADC";
    case FAULT_CJ_SENSOR:     return "CJ SENSOR";
    case FAULT_TC_RANGE:      return "TC RANGE";
    case FAULT_OVERTEMP:      return "OVERTEMP";
    case FAULT_RUNAWAY:       return "NO TEMP RISE";
    case FAULT_CONTROL_STALL: return "CTRL STALL";
    case FAULT_EXT_WDT:       return "HW WATCHDOG";
    case FAULT_INTERLOCK:     return "INTERLOCK";
    case FAULT_WDT_RESET:     return "WDT RESET";
    case FAULT_COUNT:         break;
    }
    return "?";
}
