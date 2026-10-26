/* include/storage.h */
#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#define NUM_PRESETS 3

typedef struct {
    int16_t setpoint_c;
    int16_t preset_c[NUM_PRESETS];
    uint8_t active_preset;
    int16_t standby_c;
    float tc_sens_uv_per_c;
    float tc_offset_uv;
    int8_t tc_polarity;
} settings_t;

void storage_init(void);                    // loads from flash, or defaults if nothing saved
void storage_get(settings_t *out);
void storage_update(const settings_t *in);  // gets written to flash a couple seconds later

#endif
