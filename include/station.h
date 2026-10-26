/* include/station.h */
#ifndef STATION_H
#define STATION_H

#include <stdbool.h>
#include <stdint.h>

// shared stuff between the tasks
// control task puts its status in a 1 item queue (mailbox), other tasks just peek at it
// commands go the other way through a normal queue

typedef enum {
    ST_OFF = 0,
    ST_HEATING,
    ST_READY,
    ST_STANDBY,
    ST_NO_TIP,
    ST_FAULT,
} station_state_t;

typedef enum {
    FAULT_NONE = 0,
    FAULT_ADC,              // ADS1220 not responding
    FAULT_CJ_SENSOR,        // MCP9808 not responding or bad reading
    FAULT_TC_RANGE,         // thermocouple maxed out or way too cold
    FAULT_OVERTEMP,
    FAULT_RUNAWAY,          // heating but temp not rising
    FAULT_CONTROL_STALL,    // control task stopped
    FAULT_EXT_WDT,          // 74HC123 failed self test
    FAULT_INTERLOCK,        // heater/relay interlock said no
    FAULT_WDT_RESET,        // pico got reset by its watchdog
    FAULT_COUNT
} fault_t;

typedef struct {
    station_state_t state;
    fault_t fault;
    float tip_c;
    float cj_c;             // cold junction temp
    float tc_uv;            // raw thermocouple voltage
    float duty_demand;      // what the PID wants
    float duty_applied;     // what actually got sent to the heater (0 if not armed)
    int16_t setpoint_c;
    uint8_t preset;
    bool tip_present;
    bool enabled;
    bool armed;
    uint32_t time_ms;
    uint32_t seq;
} station_status_t;

typedef enum {
    CMD_TOGGLE_ENABLE,
    CMD_SET_ENABLE,         // value = 0 or 1
    CMD_WAKE,               // user did something
    CMD_STANDBY,            // sleep timer ran out
    CMD_CLEAR_FAULT,
} station_cmd_type_t;

typedef struct {
    station_cmd_type_t type;
    int32_t value;
} station_cmd_t;

void station_ipc_init(void);

void station_publish(const station_status_t *st);
bool station_get_status(station_status_t *out);

bool station_send_cmd(station_cmd_type_t type, int32_t value);
bool station_receive_cmd(station_cmd_t *out);   // doesn't block

const char *station_state_name(station_state_t s);
const char *station_fault_name(fault_t f);

#endif
