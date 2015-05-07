Space Invaders (clone)
==============

For the LaFortuna board
--------------

An implementation of the arcade game Space Invaders for an AVR microcontroller. 
See below for the hardware for which it is developed on.
Features:
- All basic game play
- Special spaceships (at top)
- High-scores

Missing features:
- Sound
- Graphics is somewhat limited
- Houses do not explode in a pretty way
- Different game difficulties

LaFortuna hardware:
- avr90usb1286 MCU
- 240x320 screen with ILI9341 driver
- Rotary encoder with 4 directional buttons and a central button

Some code used in this project comes from different people:
- Peter Danneger's code (adapted by Klaus-Peter Zauner) is used to drive the rotary encoder
- Steve Gunn's code (adapted by Klaus-Peter Zauner and me) is used to drive the lcd

This code is released under the GPLv3 licence
