/* image.c
   Testing image display in LaFortuna
*/

#include <stdint.h>
#include <avr/io.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include "lcd.h"
#include "image.h"
#include "encoder.h"
#include "keyboard.h"

#define START_X             10
#define START_Y             LCD_HEIGHT - 100

#define K_GRID_SIZE         30
#define K_COLUMNS           10
#define K_ROWS              3

#ifndef LCD_WIDTH
#define LCD_WIDTH           320
#endif

#ifndef LCD_HEIGHT
#define LCD_HEIGHT          240
#endif

#define CHAR_HEIGHT         7
#define CHAR_WIDTH          5

#define SQUARE_OFFSET       2
#define TEXT_OFFSET         6
#define TEXT_OFFSET_Y       (K_GRID_SIZE - CHAR_HEIGHT) / 2
#define TEXT_OFFSET_X(l)    (K_GRID_SIZE - l) / 2

#define RES_STRING_X        150
#define RES_STRING_Y        50

static char lower_arr [K_GRID_SIZE] = {
    'q','w','e','r','t','y','u','i','o','p',
    0x7,'a','s','d','f','g','h','j','k','l',
    0x8,0x6,'z','x','c','v','b','n','m',0xD,
};
static char sym_arr [K_GRID_SIZE] = {
    '1','2','3','4','5','6','7','8','9','0',
    '!','"','#','$','%','&','\'','(',')','*',
    0x8,0x6,'-','_','?','>','<','=','.',0xD,
};
static char upper_arr [K_GRID_SIZE] = {
    'Q','W','E','R','T','Y','U','I','O','P',
    0x7,'A','S','D','F','G','H','J','K','L',
    0x8,0x6,'Z','X','C','V','B','N','M',0xD,
};

void draw_square(uint16_t x, uint16_t y, char data, uint16_t col);
void draw_grid(uint8_t selected, uint8_t last);

char k_str[MAX_STRING_SIZE + 1]; // allow for null termination
volatile uint8_t sel, last_sel;
volatile uint8_t string_pos, last_string_pos;
volatile char *curr_array;

typedef struct {
    uint16_t x, y;
    uint8_t alive;
} sprite;

uint8_t move_keyboard() {
    if(get_switch_short(_BV(SWC))) {
        char data = curr_array[sel];
        if(data == 0x8) { //Backspace
            if(string_pos > 0) {
                string_pos--;
                k_str[string_pos] = '\0';
            }
        } else if(data == 0xD) { //Enter
            clear_switches();
            return 1;
        } else if(data == 0x6) { //Switch alphabet/symbols
            if(curr_array == sym_arr) {
                curr_array = lower_arr;
            } else {
                curr_array = sym_arr;
            }
            last_sel = 0xFF; //Force redraw
        } else if(data == 0x7) { //Switch upper/lower
            if(curr_array == lower_arr) {
                curr_array = upper_arr;
            } else {
                curr_array = lower_arr;
            }
            last_sel = 0xFF; //Force redraw
        } else if(string_pos < MAX_STRING_SIZE) {
            k_str[string_pos] = data;
            string_pos++;
        }
    }
    if(get_switch_short(_BV(SWN))) { //Top (no wrap)
        if(sel >= K_COLUMNS)
            sel -= K_COLUMNS;
    }
    if(get_switch_short(_BV(SWE))) { //Right (wrap)
        if(sel < (K_GRID_SIZE - 1))
            sel++;
    }
    if(get_switch_short(_BV(SWS)) && //Bottom (no wrap)
        sel < (K_COLUMNS * (K_ROWS - 1))) {
        sel += K_COLUMNS;
    }
    if(get_switch_short(_BV(SWW)) && //Left (wrap)
        sel > 0) {
        sel--;
    }
    int8_t rot = os_enc_delta();
    if(rot < 0 && sel > 0) {
        sel--;
    } else if(rot > 0 && sel < (K_GRID_SIZE - 1)) {
        sel++;
    }
    return 0;
}

void draw_square(uint16_t x, uint16_t y, char data, uint16_t col) {
    //Fill background
    fill_rectangle_c(x + SQUARE_OFFSET, y + SQUARE_OFFSET, K_GRID_SIZE - SQUARE_OFFSET, K_GRID_SIZE - SQUARE_OFFSET, col);
    if(data == 0x8) { //Backspace
        uint8_t off = TEXT_OFFSET_X(2 * (CHAR_WIDTH + 1)) + x + SQUARE_OFFSET;
        display_char_xy_col('<', off, y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
        display_char_xy_col('-', (off += CHAR_WIDTH + 1), y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
    } else if(data == 0xD) { //Enter
        display_char_xy_col('E', x + SQUARE_OFFSET + 1, y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
        display_char_xy_col('n', x + SQUARE_OFFSET + 6, y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
        display_char_xy_col('t', x + SQUARE_OFFSET + 11, y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
        display_char_xy_col('e', x + SQUARE_OFFSET + 16, y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
        display_char_xy_col('r', x + SQUARE_OFFSET + 21, y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
    } else if(data == 0x6) { //Switch alphabet/symbols
        if(curr_array == sym_arr) {
            uint8_t off = TEXT_OFFSET_X(3 * (CHAR_WIDTH + 1)) + x + SQUARE_OFFSET;
            display_char_xy_col('a', off, y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
            display_char_xy_col('b', (off += (CHAR_WIDTH + 1)), y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
            display_char_xy_col('c', (off += (CHAR_WIDTH + 1)), y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
        } else {
            uint8_t off = TEXT_OFFSET_X(3 * (CHAR_WIDTH + 1)) + x + SQUARE_OFFSET;
            display_char_xy_col('.', off, y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
            display_char_xy_col('?', (off += CHAR_WIDTH + 1), y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
            display_char_xy_col('*', (off += CHAR_WIDTH + 1), y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
        }
    } else if(data == 0x7) { //Switch upper/lower
        if(curr_array == lower_arr) {
            uint8_t off = TEXT_OFFSET_X(3 * (CHAR_WIDTH + 1)) + x + SQUARE_OFFSET;
            display_char_xy_col('A', off, y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
            display_char_xy_col('B', (off += CHAR_WIDTH + 1), y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
            display_char_xy_col('C', (off += CHAR_WIDTH + 1), y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
        } else {
            uint8_t off = TEXT_OFFSET_X(3 * (CHAR_WIDTH + 1)) + x + SQUARE_OFFSET;
            display_char_xy_col('a', off, y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
            display_char_xy_col('b', (off += CHAR_WIDTH + 1), y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
            display_char_xy_col('c', (off += CHAR_WIDTH + 1), y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
        }
    } else {
        display_char_xy_col(data, x + SQUARE_OFFSET + TEXT_OFFSET_X(CHAR_WIDTH + 3), y + SQUARE_OFFSET + TEXT_OFFSET_Y, WHITE, col);
    }
}

void draw_grid(uint8_t selected, uint8_t last) {
    uint8_t x, y, i;
    uint16_t x_offset, y_offset;
    if(last >= K_ROWS * K_COLUMNS) {
        y_offset = START_Y;
        for(y = 0, i = 0; y < K_ROWS; y++, y_offset += K_GRID_SIZE) {
            for(x = 0, x_offset = START_X; x < K_COLUMNS; x++, i++, x_offset += K_GRID_SIZE) {
                if(i == selected)
                    draw_square(x_offset, y_offset, curr_array[i], BLUE);
                else
                    draw_square(x_offset, y_offset, curr_array[i], GRAY);
            }
        }
    } else if(last != selected) {
        //clear last
        x_offset = (last % K_COLUMNS) * K_GRID_SIZE + START_X;
        y_offset = (last / K_COLUMNS) * K_GRID_SIZE + START_Y;
        draw_square(x_offset, y_offset, curr_array[last], GRAY);
        //draw selected
        x_offset = (selected % K_COLUMNS) * K_GRID_SIZE + START_X;
        y_offset = (selected / K_COLUMNS) * K_GRID_SIZE + START_Y;
        draw_square(x_offset, y_offset, curr_array[selected], BLUE);
    }
}

void init_keyboard() {
    uint8_t i;
    last_sel = 0xFF;
    curr_array = lower_arr;
    sel = 0;
    for(i = 0; i < MAX_STRING_SIZE; i++) {
        k_str[i] = '\0';
    }
}

void draw_keyboard() {
    if(sel != last_sel) {
        draw_grid(sel, last_sel);
        last_sel = sel;
    }
    if(string_pos < last_string_pos) {
        fill_rectangle_c(RES_STRING_X,RES_STRING_Y,MAX_STRING_SIZE * (CHAR_WIDTH + 2),CHAR_HEIGHT, display.background);
    }
    if(string_pos != last_string_pos) {
        display_string_xy_col(k_str, RES_STRING_X, RES_STRING_Y, WHITE);
        last_string_pos = string_pos;
    }
}
