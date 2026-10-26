/* src/ssd1306.c */
#include "ssd1306.h"

#include <string.h>

#include "i2c_bus.h"
#include "font5x7.h"

// 0.96" OLEDs are almost always SSD1306. if the picture comes out shifted it
// might be an SH1106 instead and this will need changing

#define ADDR 0x3C

static uint8_t s_fb[OLED_WIDTH * OLED_HEIGHT / 8];  // framebuffer, 1 bit per pixel

static bool send_cmds(const uint8_t *cmds, size_t n) {
    uint8_t buf[32];
    buf[0] = 0x00;      // 0x00 = commands follow
    memcpy(&buf[1], cmds, n);
    return i2c_bus_write(ADDR, buf, n + 1, false);
}

bool ssd1306_init(void) {
    // standard init sequence for 128x64
    static const uint8_t init[] = {
        0xAE,           // display off
        0xD5, 0x80,     // clock
        0xA8, 0x3F,     // 64 rows
        0xD3, 0x00,     // no offset
        0x40,           // start line 0
        0x8D, 0x14,     // charge pump on
        0x20, 0x00,     // horizontal addressing mode
        0xA1,           // flip horizontally
        0xC8,           // flip vertically
        0xDA, 0x12,
        0x81, 0xCF,     // contrast
        0xD9, 0xF1,
        0xDB, 0x40,
        0xA4,
        0xA6,           // not inverted
        0x2E,           // no scrolling
        0xAF,           // display on
    };
    if (!i2c_bus_lock(pdMS_TO_TICKS(200))) return false;
    bool ok = send_cmds(init, sizeof init);
    i2c_bus_unlock();
    ssd1306_clear();
    return ok && ssd1306_flush();
}

void ssd1306_clear(void) {
    memset(s_fb, 0, sizeof s_fb);
}

void ssd1306_pixel(int x, int y, bool on) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    uint8_t *p = &s_fb[(y >> 3) * OLED_WIDTH + x];
    uint8_t m = (uint8_t)(1u << (y & 7));
    if (on) *p |= m;
    else *p &= (uint8_t)~m;
}

void ssd1306_hline(int x0, int x1, int y) {
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    for (int x = x0; x <= x1; x++) ssd1306_pixel(x, y, true);
}

void ssd1306_vline(int x, int y0, int y1) {
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int y = y0; y <= y1; y++) ssd1306_pixel(x, y, true);
}

// bresenham line
void ssd1306_line(int x0, int y0, int x1, int y1) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        ssd1306_pixel(x0, y0, true);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void ssd1306_rect(int x, int y, int w, int h, bool fill) {
    if (w <= 0 || h <= 0) return;
    if (fill) {
        for (int j = y; j < y + h; j++) ssd1306_hline(x, x + w - 1, j);
    } else {
        ssd1306_hline(x, x + w - 1, y);
        ssd1306_hline(x, x + w - 1, y + h - 1);
        ssd1306_vline(x, y, y + h - 1);
        ssd1306_vline(x + w - 1, y, y + h - 1);
    }
}

static void draw_char(int x, int y, char c, int scale) {
    if (c < FONT5X7_FIRST || c > FONT5X7_LAST) c = '?';
    const uint8_t *g = font5x7[c - FONT5X7_FIRST];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 8; row++) {
            if (g[col] & (1u << row)) {
                if (scale == 1) ssd1306_pixel(x + col, y + row, true);
                else ssd1306_rect(x + col * scale, y + row * scale, scale, scale, true);
            }
        }
    }
}

int ssd1306_text(int x, int y, const char *s, int scale) {
    while (*s) {
        draw_char(x, y, *s++, scale);
        x += 6 * scale;
    }
    return x;
}

int ssd1306_text_width(const char *s, int scale) {
    return (int)strlen(s) * 6 * scale;
}

bool ssd1306_flush(void) {
    // send one page (8 rows) at a time and let go of the bus in between.
    // a whole frame takes ~23ms and the control task needs the bus for the MCP9808
    static uint8_t buf[1 + OLED_WIDTH];
    bool ok = true;
    for (int page = 0; page < OLED_HEIGHT / 8 && ok; page++) {
        if (!i2c_bus_lock(pdMS_TO_TICKS(50))) return false;
        const uint8_t win[] = { 0x21, 0, OLED_WIDTH - 1, 0x22, (uint8_t)page, 7 };
        ok = send_cmds(win, sizeof win);
        if (ok) {
            buf[0] = 0x40;      // 0x40 = display data follows
            memcpy(&buf[1], &s_fb[page * OLED_WIDTH], OLED_WIDTH);
            ok = i2c_bus_write(ADDR, buf, sizeof buf, false);
        }
        i2c_bus_unlock();
    }
    return ok;
}
