/*
  Keyboard.h
  On screen keyboard for AVR90USB1286
*/
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#define MAX_STRING_SIZE     10

//The string which is inputted from the keyboard.
extern char k_str[MAX_STRING_SIZE + 1];

/*
  Detects user motion on the keyboard (using encoder.h),
  and updates state. Should be called often. 
  Returns true if Enter was pressed.
*/
uint8_t move_keyboard();

/*
  Draw the keyboard on screen using lcd.h
  Call it every time you want to update the screen (often)
*/
void draw_keyboard();

/*
  Initialize the keyboard. Call before displaying it.
*/
void init_keyboard();

#endif /* KEYBOARD_H */