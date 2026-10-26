/* src/ui_task.c */
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "tasks.h"
#include "app_config.h"
#include "station.h"
#include "input.h"
#include "ssd1306.h"
#include "storage.h"

// Controls:
//   encoder     - setpoint up/down 5C
//   PSH         - heater on/off (or clear fault if there is one)
//   CON         - next preset
//   hold CON    - save setpoint to the current preset
//   BAK         - switch between number and graph screen

#define GRAPH_T_MAX_C 500
#define GRAPH_TOP 11

typedef enum { VIEW_NUMBER, VIEW_GRAPH } view_t;

static int16_t s_hist[OLED_WIDTH];  // temp history for the graph
static int s_hist_len;
static int s_hist_head;
static view_t s_view = VIEW_NUMBER;
static char s_msg[22];              // popup message
static TickType_t s_msg_until;

static void flash_msg(const char *m) {
    snprintf(s_msg, sizeof s_msg, "%s", m);
    s_msg_until = xTaskGetTickCount() + pdMS_TO_TICKS(1200);
}

static void handle_event(input_event_t ev, const station_status_t *st) {
    settings_t s;
    storage_get(&s);
    char buf[22];

    switch (ev) {
    case EV_ENC_CW:
    case EV_ENC_CCW: {
        int sp = s.setpoint_c + (ev == EV_ENC_CW ? SETPOINT_STEP_C : -SETPOINT_STEP_C);
        if (sp < SETPOINT_MIN_C) sp = SETPOINT_MIN_C;
        if (sp > SETPOINT_MAX_C) sp = SETPOINT_MAX_C;
        s.setpoint_c = (int16_t)sp;
        storage_update(&s);
        break;
    }
    case EV_PSH:
        station_send_cmd(st->state == ST_FAULT ? CMD_CLEAR_FAULT : CMD_TOGGLE_ENABLE, 0);
        break;
    case EV_CON:
        s.active_preset = (uint8_t)((s.active_preset + 1) % NUM_PRESETS);
        s.setpoint_c = s.preset_c[s.active_preset];
        storage_update(&s);
        snprintf(buf, sizeof buf, "Preset %d: %dC", s.active_preset + 1, s.setpoint_c);
        flash_msg(buf);
        break;
    case EV_CON_LONG:
        s.preset_c[s.active_preset] = s.setpoint_c;
        storage_update(&s);
        snprintf(buf, sizeof buf, "Saved P%d = %dC", s.active_preset + 1, s.setpoint_c);
        flash_msg(buf);
        break;
    case EV_BAK:
        s_view = (s_view == VIEW_NUMBER) ? VIEW_GRAPH : VIEW_NUMBER;
        break;
    case EV_PSH_LONG:
    case EV_BAK_LONG:
        break;  // TODO: use these for a calibration menu
    }
    station_send_cmd(CMD_WAKE, 0);  // any button resets the sleep timer
}

// top line: state on the left, preset on the right
static void draw_header(const station_status_t *st) {
    char buf[22];
    if (st->state == ST_FAULT) snprintf(buf, sizeof buf, "FAULT %s", station_fault_name(st->fault));
    else snprintf(buf, sizeof buf, "%s", station_state_name(st->state));
    ssd1306_text(0, 0, buf, 1);

    // "DRY" = heater not armed
    snprintf(buf, sizeof buf, "%sP%d", st->armed ? "" : "DRY ", st->preset + 1);
    ssd1306_text(OLED_WIDTH - ssd1306_text_width(buf, 1), 0, buf, 1);
}

static void draw_number_view(const station_status_t *st) {
    char buf[22];
    if (st->tip_present) snprintf(buf, sizeof buf, "%3d", (int)(st->tip_c + 0.5f));
    else snprintf(buf, sizeof buf, "---");
    int x = ssd1306_text(8, 14, buf, 3);
    ssd1306_text(x + 2, 14, "C", 2);

    snprintf(buf, sizeof buf, "SET %dC", st->setpoint_c);
    ssd1306_text(0, 42, buf, 1);
    snprintf(buf, sizeof buf, "CJ %dC", (int)(st->cj_c + 0.5f));
    ssd1306_text(OLED_WIDTH - ssd1306_text_width(buf, 1), 42, buf, 1);

    // duty bar at the bottom
    int pct = (int)(st->duty_demand * 100.0f + 0.5f);
    ssd1306_rect(0, 54, 100, 9, false);
    ssd1306_rect(1, 55, pct * 98 / 100, 7, true);
    snprintf(buf, sizeof buf, "%d%%", pct);
    ssd1306_text(OLED_WIDTH - ssd1306_text_width(buf, 1), 55, buf, 1);
}

static int temp_to_y(int t) {
    if (t < 0) t = 0;
    if (t > GRAPH_T_MAX_C) t = GRAPH_T_MAX_C;
    return (OLED_HEIGHT - 1) - t * (OLED_HEIGHT - 1 - GRAPH_TOP) / GRAPH_T_MAX_C;
}

static void draw_graph_view(const station_status_t *st) {
    // dotted line for the setpoint
    int ysp = temp_to_y(st->setpoint_c);
    for (int x = 0; x < OLED_WIDTH; x += 4) ssd1306_pixel(x, ysp, true);

    // oldest sample on the left, newest on the right
    int start = (s_hist_head - s_hist_len + OLED_WIDTH) % OLED_WIDTH;
    int x0 = OLED_WIDTH - s_hist_len;
    int py = -1;
    for (int i = 0; i < s_hist_len; i++) {
        int y = temp_to_y(s_hist[(start + i) % OLED_WIDTH]);
        if (py >= 0) ssd1306_line(x0 + i - 1, py, x0 + i, y);
        else ssd1306_pixel(x0 + i, y, true);
        py = y;
    }

    char buf[22];
    snprintf(buf, sizeof buf, "%dC", (int)(st->tip_c + 0.5f));
    ssd1306_text(0, GRAPH_TOP + 1, buf, 1);
}

static void ui_task(void *arg) {
    (void)arg;
    bool oled_ok = ssd1306_init();
    TickType_t next_sample = xTaskGetTickCount();

    for (;;) {
        station_status_t st = {0};
        station_get_status(&st);

        // wait for button events (up to one frame), then redraw
        input_event_t ev;
        TickType_t wait = pdMS_TO_TICKS(UI_FRAME_MS);
        while (input_get_event(&ev, wait)) {
            handle_event(ev, &st);
            wait = 0;
        }
        station_get_status(&st);

        if ((int32_t)(xTaskGetTickCount() - next_sample) >= 0) {
            next_sample += pdMS_TO_TICKS(GRAPH_SAMPLE_MS);
            s_hist[s_hist_head] = (int16_t)st.tip_c;
            s_hist_head = (s_hist_head + 1) % OLED_WIDTH;
            if (s_hist_len < OLED_WIDTH) s_hist_len++;
        }

        // keep trying if the screen wasnt found
        if (!oled_ok) {
            oled_ok = ssd1306_init();
            if (!oled_ok) continue;
        }

        ssd1306_clear();
        draw_header(&st);
        if (s_view == VIEW_NUMBER) draw_number_view(&st);
        else draw_graph_view(&st);
        if ((int32_t)(s_msg_until - xTaskGetTickCount()) > 0) {
            // popup box in the middle of the screen
            ssd1306_rect(0, 30, OLED_WIDTH, 12, true);
            for (int y = 31; y < 41; y++)
                for (int x = 1; x < OLED_WIDTH - 1; x++) ssd1306_pixel(x, y, false);
            ssd1306_text(3, 32, s_msg, 1);
        }
        oled_ok = ssd1306_flush();
    }
}

void ui_task_start(void) {
    BaseType_t ok = xTaskCreate(ui_task, "ui", STACK_UI, NULL, PRIO_UI, NULL);
    configASSERT(ok == pdPASS);
}
