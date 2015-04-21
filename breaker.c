/*
 *
 * SPACE INVADERS FOR AVR90USB1286
 *
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "lcd.h"
#include "encoder.h"

#define LED_INIT    DDRB  |=  _BV(PINB7)
#define LED_ON      PORTB |=  _BV(PINB7)
#define LED_OFF     PORTB &= ~_BV(PINB7) 

#define CANNON_WIDTH    40
#define CANNON_HEIGHT   10
#define CANNON_SPEED    4

#define LASER_WIDTH     1
#define LASER_HEIGHT    4

#define MONSTER_TOP     20
#define MONSTER_PADDING_X 8
#define MONSTER_PADDING_Y 3
#define MONSTER_WIDTH   30
#define MONSTER_HEIGHT  10
#define MONSTERS_X      6
#define MONSTERS_Y      5
#define MONSTER_SPEED   1
//MONSTERS_X * (MONSTER_PADDING_X + MONSTER_WIDTH) = 6*(30+8) = 228 < LCDWIDTH

#define MAX_LASERS      7
#define LASER_SPEED     1

#define LCDWIDTH        320
#define LCDHEIGHT       240

#define FALSE           0
#define TRUE            1

typedef struct {
    rectangle rect;
    uint8_t alive;
} sprite;

const sprite start_cannon = {{(LCDWIDTH-CANNON_WIDTH)/2, (LCDWIDTH+CANNON_WIDTH)/2, LCDHEIGHT-CANNON_HEIGHT-1, LCDHEIGHT-1}, 1};

volatile sprite lasers[MAX_LASERS];
volatile sprite monsters[6][5];
volatile sprite cannon;
volatile sprite last_monsters[6][5];
volatile sprite last_lasers[MAX_LASERS];
volatile sprite last_cannon;

volatile uint16_t leftmost, rightmost;
volatile uint16_t score;
volatile uint8_t lives;
volatile uint8_t has_monsters;
volatile uint8_t fps = 0;

uint8_t intersect_rect(rectangle r1, rectangle r2);

ISR(TIMER1_COMPA_vect)
{
    uint8_t x, y, l;
    static int8_t xinc = MONSTER_SPEED;
    for(l = 0; l < MAX_LASERS; l++) {
        if(lasers[l].rect.top <= LASER_SPEED) {
            lasers[l].alive = FALSE;
            continue;
        }
        if(lasers[l].alive && 
            lasers[l].rect.right >= leftmost &&
            lasers[l].rect.left <= rightmost) {
            //Collision check
            for(x = 0; x < MONSTERS_X; x++) {
                for(y = 0; y < MONSTERS_Y; y++) {
                    if(monsters[x][y].alive && 
                      intersect_rect(lasers[l].rect, monsters[x][y].rect)) {
                        lasers[l].alive = FALSE;
                        monsters[x][y].alive = FALSE;
                    }
                }
            }
        }
        //Move lasers
        if(lasers[l].alive) {
            lasers[l].rect.bottom -= LASER_SPEED;
            lasers[l].rect.top -= LASER_SPEED;
        }
    }
    rightmost = 0;
    leftmost = LCDWIDTH;
    for(x = 0; x < MONSTERS_X; x++) {
        for(y = 0; y < MONSTERS_Y; y++) {
            if(monsters[x][y].alive) {
                if(monsters[x][y].rect.right > rightmost)
                    rightmost = monsters[x][y].rect.right;
                if(monsters[x][y].rect.left < leftmost)
                    leftmost = monsters[x][y].rect.left;
            }
        }
    }
    uint8_t yinc = 0;
    //Move monsters
    if(leftmost < MONSTER_PADDING_X || rightmost > LCDWIDTH - MONSTER_PADDING_X) {
        xinc = -xinc;
        yinc = MONSTER_SPEED;
    }
    has_monsters = 0;
    for(x = 0; x < MONSTERS_X; x++) {
        for(y = 0; y < MONSTERS_Y; y++) {
            if(monsters[x][y].alive) {
                monsters[x][y].rect.left += xinc;
                monsters[x][y].rect.right += xinc;
                monsters[x][y].rect.top += yinc;
                monsters[x][y].rect.bottom += yinc;
                if(monsters[x][y].rect.bottom >= cannon.rect.top) {
                    lives = 0;
                }
                has_monsters = 1;
            }
        }
    }
    //Move cannon
    int8_t rotary = os_enc_delta();
    if(rotary < 0 && cannon.rect.left > 0) {
        cannon.rect.left -= CANNON_SPEED;
        cannon.rect.right -= CANNON_SPEED;
    }
    else if (rotary > 0 && cannon.rect.right < LCDWIDTH) {
        cannon.rect.left += CANNON_SPEED;
        cannon.rect.right += CANNON_SPEED;
    }
    //Shoot
    if(get_switch_short(_BV(SWC))) {
        for(l = 0; l < MAX_LASERS; l++) {
            if(!lasers[l].alive) {
                uint8_t left = (cannon.rect.left + cannon.rect.right)/2 - LASER_WIDTH/2;
                lasers[l].rect.left = left;
                lasers[l].rect.right = left + LASER_WIDTH;
                lasers[l].rect.top = cannon.rect.top - LASER_HEIGHT;
                lasers[l].rect.bottom = cannon.rect.top;
                lasers[l].alive = 1;
                break;
            }
        }
    }
}

ISR(INT6_vect) {
    uint8_t x, y, l;
    for(x = 0; x < MONSTERS_X; x++) {
        for(y = 0; y < MONSTERS_Y; y++) {
            if(monsters[x][y].alive) {
                fill_rectangle(last_monsters[x][y].rect, display.background);
                fill_rectangle(monsters[x][y].rect, RED);
            } else if(last_monsters[x][y].alive) { //Has just died
                fill_rectangle(last_monsters[x][y].rect, display.background);
            }
            last_monsters[x][y] = monsters[x][y];
        }
    }
    for(l = 0; l < MAX_LASERS; l++) {
        if(lasers[l].alive) {
            fill_rectangle(last_lasers[l].rect, display.background);
            fill_rectangle(lasers[l].rect, BLUE);
        } else if(last_lasers[l].alive) { //Has just died
            fill_rectangle(last_lasers[l].rect, display.background);
        }
        last_lasers[l] = lasers[l];
    }
    fill_rectangle(last_cannon.rect, display.background);
    fill_rectangle(cannon.rect, GREEN);
    last_cannon = cannon;
	//fps++;
}

ISR(TIMER3_COMPA_vect)
{
    scan_switches();
    scan_encoder();
	/*char buffer[4];
	sprintf(buffer, "%03d", fps);
	display_string_xy(buffer, 10, 2);
	fps = 0;*/
}

void os_init(void) {
	/* 8MHz clock, no prescaling (DS, p. 48) */
    CLKPR = (1 << CLKPCE);
    CLKPR = 0;

	LED_INIT;
    init_lcd();
    init_encoder();
    
    /* Frame rate */
	set_frame_rate_hz(31); /* > 60 Hz  (KPZ 30.01.2015) */
    
	/* Enable tearing interrupt to get flicker free display */
	EIMSK |= _BV(INT6);
	/* Enable game timer interrupt (Timer 1 CTC Mode 4) */
	TCCR1A = 0;
    //CTC: TOP=OCR1A; don't use TOV1 (it's set at MAX=0xffff)
	TCCR1B = _BV(WGM12);
	TCCR1B |= _BV(CS10); //No prescaling
	TIMSK1 |= _BV(OCIE1A);
	/* Enable performance counter (Timer 3 CTC Mode 4) */
	TCCR3A = 0;
	TCCR3B = _BV(WGM32);
	TCCR3B |= _BV(CS31); // clk/8
	TIMSK3 |= _BV(OCIE3A);
	OCR3A = 2000;       // trigger interrupt every 2ms
}

int main() {
    os_init();
    uint8_t x, y, l;
    do {
        for(x = 0; x < MONSTERS_X; x++) {
            for(y = 0; y < MONSTERS_Y; y++) {
                uint16_t left = x*(MONSTER_WIDTH+MONSTER_PADDING_X)+MONSTER_PADDING_X;
                uint16_t top = y*(MONSTER_HEIGHT+MONSTER_PADDING_Y)+MONSTER_TOP;
                monsters[x][y].rect.left = left;
                monsters[x][y].rect.right = left + MONSTER_WIDTH;
                monsters[x][y].rect.top = top;
                monsters[x][y].rect.bottom = top + MONSTER_HEIGHT;
                monsters[x][y].alive = 1;
            }
        }
        has_monsters = 1;
        cannon = start_cannon;
        leftmost = MONSTER_PADDING_X;
        rightmost = MONSTERS_X*(MONSTER_WIDTH+MONSTER_PADDING_X);
        lives = 3;
        score = 0;
		OCR1A = 65535;
        LED_ON;
        sei();
        while(lives && has_monsters);
        cli();
        LED_OFF;
        if(!lives) {
            display_string_xy("Game Over", 90, 150);
        } else {
            display_string_xy("YOU WIN!", 90, 150);
        }
        PORTB |= _BV(PB6);
        while(PINE & _BV(SWC)) {
            if(PINB % _BV(PB6))
                LED_ON;
            else
                LED_OFF;
        }            
        for(l = 0; l < MAX_LASERS; l++) {
            lasers[l].alive = 0;
        }
        clear_screen();
    } while(1);
}

uint8_t intersect_rect(rectangle r1, rectangle r2) {
    return !(r2.left > r1.right
        || r2.right < r1.left
        || r2.top > r1.bottom
        || r2.bottom < r1.top);
}