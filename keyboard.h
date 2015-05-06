/*
  Keyboard.h
  On screen keyboard for AVR90USB1286
*/

#define MAX_STRING_SIZE     10

extern char k_str[MAX_STRING_SIZE + 1];

//Returns true if Enter was pressed
uint8_t move_keyboard();
void draw_keyboard();
void init_keyboard();