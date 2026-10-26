/* include/ssd1306.h */
#ifndef SSD1306_H
#define SSD1306_H

#include <stdbool.h>
#include <stdint.h>

#define OLED_WIDTH  128
#define OLED_HEIGHT 64

bool ssd1306_init(void);
void ssd1306_clear(void);
void ssd1306_pixel(int x, int y, bool on);
void ssd1306_hline(int x0, int x1, int y);
void ssd1306_vline(int x, int y0, int y1);
void ssd1306_line(int x0, int y0, int x1, int y1);
void ssd1306_rect(int x, int y, int w, int h, bool fill);
int ssd1306_text(int x, int y, const char *s, int scale);   // returns x where the text ended
int ssd1306_text_width(const char *s, int scale);
bool ssd1306_flush(void);

#endif
