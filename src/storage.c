/* src/storage.c */
#include "storage.h"

#include <stddef.h>
#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "timers.h"

#include "app_config.h"
#include "ext_watchdog.h"
#include "heater.h"

// Settings are stored in the last 4KB of the Pico's 2MB flash. The sector is split
// into 16 pages of 256 bytes. Each save goes in the next blank page and it only
// erases when all 16 are used. when the pico boots the valid page with the
// highest sequence number is used.

#define STORE_OFFSET   (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define SLOTS          (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE)
#define RECORD_MAGIC   0xC245C245u

typedef struct {
    uint32_t magic;
    uint32_t seq;
    settings_t s;
    uint32_t crc;
} record_t;

_Static_assert(sizeof(record_t) <= FLASH_PAGE_SIZE, "settings record must fit in one flash page");

static settings_t s_settings;
static settings_t s_saved;          // what's currently in flash
static uint32_t s_seq;
static SemaphoreHandle_t s_mutex;
static TimerHandle_t s_save_timer;

static uint32_t crc32(const void *data, size_t len) {
    const uint8_t *p = data;
    uint32_t crc = 0xFFFFFFFFu;
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1));
    }
    return ~crc;
}

static const record_t *slot_ptr(int i) {
    return (const record_t *)(XIP_BASE + STORE_OFFSET + (uint32_t)i * FLASH_PAGE_SIZE);
}

static bool slot_valid(const record_t *r) {
    return r->magic == RECORD_MAGIC && r->crc == crc32(r, offsetof(record_t, crc));
}

static bool slot_blank(int i) {
    const uint8_t *p = (const uint8_t *)slot_ptr(i);
    for (unsigned k = 0; k < FLASH_PAGE_SIZE; k++)
        if (p[k] != 0xFF) return false;
    return true;
}

static void defaults(settings_t *s) {
    memset(s, 0, sizeof *s);
    s->setpoint_c = SETPOINT_DEFAULT_C;
    s->preset_c[0] = 280;
    s->preset_c[1] = 320;
    s->preset_c[2] = 360;
    s->active_preset = 1;
    s->standby_c = STANDBY_TEMP_C;
    s->tc_sens_uv_per_c = TC_SENS_UV_PER_C;
    s->tc_offset_uv = TC_OFFSET_UV;
    s->tc_polarity = TC_POLARITY;
}

// fix anything out of range (ex. garbage from an old version of the struct)
static void sanitize(settings_t *s) {
    if (s->setpoint_c < SETPOINT_MIN_C || s->setpoint_c > SETPOINT_MAX_C)
        s->setpoint_c = SETPOINT_DEFAULT_C;
    for (int i = 0; i < NUM_PRESETS; i++)
        if (s->preset_c[i] < SETPOINT_MIN_C || s->preset_c[i] > SETPOINT_MAX_C)
            s->preset_c[i] = SETPOINT_DEFAULT_C;
    if (s->active_preset >= NUM_PRESETS)
        s->active_preset = 0;
    if (s->standby_c < SETPOINT_MIN_C || s->standby_c > SETPOINT_MAX_C)
        s->standby_c = STANDBY_TEMP_C;
    if (!(s->tc_sens_uv_per_c > 5.0f && s->tc_sens_uv_per_c < 100.0f))
        s->tc_sens_uv_per_c = TC_SENS_UV_PER_C;
    if (s->tc_polarity != 1 && s->tc_polarity != -1)
        s->tc_polarity = TC_POLARITY;
}

static void save_now(void) {
    settings_t snap;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    snap = s_settings;
    xSemaphoreGive(s_mutex);
    if (memcmp(&snap, &s_saved, sizeof snap) == 0) return;  // nothing changed

    static uint8_t page[FLASH_PAGE_SIZE];
    memset(page, 0xFF, sizeof page);
    record_t *r = (record_t *)page;
    r->magic = RECORD_MAGIC;
    r->seq = ++s_seq;
    r->s = snap;
    r->crc = crc32(r, offsetof(record_t, crc));

    int slot = -1;
    for (int i = 0; i < (int)SLOTS; i++)
        if (slot_blank(i)) { slot = i; break; }

    // interrupts are off while writing flash and an erase takes ~45ms,
    // so turn the heater off and feed both watchdogs first
    heater_off();       // not force_off, the control task handles the relays
    ext_wdt_kick();
    watchdog_update();

    uint32_t irq = save_and_disable_interrupts();
    if (slot < 0) {
        flash_range_erase(STORE_OFFSET, FLASH_SECTOR_SIZE);
        slot = 0;
    }
    flash_range_program(STORE_OFFSET + (uint32_t)slot * FLASH_PAGE_SIZE, page, FLASH_PAGE_SIZE);
    restore_interrupts(irq);

    s_saved = snap;
}

static void save_timer_cb(TimerHandle_t t) {
    (void)t;
    save_now();
}

void storage_init(void) {
    s_mutex = xSemaphoreCreateMutex();
    configASSERT(s_mutex);

    // find the newest valid save
    const record_t *best = NULL;
    for (int i = 0; i < (int)SLOTS; i++) {
        const record_t *r = slot_ptr(i);
        if (slot_valid(r) && (!best || r->seq > best->seq)) best = r;
    }
    if (best) {
        s_settings = best->s;
        s_seq = best->seq;
    } else {
        defaults(&s_settings);
        s_seq = 0;
    }
    sanitize(&s_settings);
    s_saved = s_settings;

    s_save_timer = xTimerCreate("save", pdMS_TO_TICKS(SETTINGS_SAVE_DELAY_MS), pdFALSE, NULL, save_timer_cb);
    configASSERT(s_save_timer);
}

void storage_get(settings_t *out) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_settings;
    xSemaphoreGive(s_mutex);
}

void storage_update(const settings_t *in) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_settings = *in;
    sanitize(&s_settings);
    xSemaphoreGive(s_mutex);
    xTimerReset(s_save_timer, 0);   // restarts the 2s wait so it saves once you stop turning the knob
}
