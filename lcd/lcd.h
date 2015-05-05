/*  Author: Steve Gunn
 * Licence: This work is licensed under the Creative Commons Attribution License.
 *           View this license at http://creativecommons.org/about/licenses/
 */
 
#include <avr/io.h>
#include <stdint.h>

#define LCDWIDTH	240
#define LCDHEIGHT	320

/* Colour definitions RGB565 */
#define WHITE       0xFFFF
#define BLACK       0x0000

typedef enum {North, West, South, East} orientation;

typedef struct {
	uint16_t width, height;
	orientation orient;
	uint16_t x, y;
	uint16_t foreground, background;
} lcd;

extern lcd display;

typedef struct {
	uint16_t left, right;
	uint16_t top, bottom;
} rectangle;	

void init_lcd();
void lcd_brightness(uint8_t i);
void set_orientation(orientation o);
void set_frame_rate_hz(uint8_t f);
void clear_screen();
void fill_rectangle(rectangle r, uint16_t col);
void fill_rectangle_c(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t col);
void draw_vline(uint16_t x, uint16_t y, uint16_t h, uint16_t col);
void fill_rectangle_indexed(rectangle r, uint16_t* col);
void display_char_col(char c, uint16_t col);
void display_char(char c);
void display_string_col(char *str, uint16_t col);
void display_string(char *str);
void display_string_xy(char *str, uint16_t x, uint16_t y);
void display_string_xy_col(char *str, uint16_t x, uint16_t y, uint16_t col);
void display_register(uint8_t reg);
void fill_image(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t *col);
void fill_image_pgm(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t *col);
void start_rectangle_fill(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void write_pixel(uint16_t color);
void fill_image_pgm_2b(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t *col);
