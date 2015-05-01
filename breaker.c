/*
 *
 * SPACE INVADERS FOR AVR90USB1286
 *
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <util/delay.h>
#include "lcd.h"
#include "encoder.h"
#include "image.h"

#define LED_INIT    DDRB  |=  _BV(PINB7)
#define LED_ON      PORTB |=  _BV(PINB7)
#define LED_OFF     PORTB &= ~_BV(PINB7) 

#define CANNON_WIDTH        26
#define CANNON_HEIGHT       10
#define CANNON_SPEED        4

#define LASER_WIDTH         1
#define LASER_HEIGHT        4

#define MONSTER_TOP         20
#define MONSTER_PADDING_X   10
#define MONSTER_PADDING_Y   9
#define MONSTER_WIDTH       22
#define MONSTER_HEIGHT      16
#define MONSTERS_X          6
#define MONSTERS_Y          4
#define MONSTER_SPEED       8
//MONSTERS_X * (MONSTER_PADDING_X + MONSTER_WIDTH) = 6*(30+8) = 228 < LCDWIDTH

//#define MAX_LASERS          7
#define LASER_SPEED         1
#define MAX_MONSTER_LASERS  5

#define HOUSE_WIDTH         30
#define HOUSE_HEIGHT        25
#define HOUSE_LEG_WIDTH     8
#define HOUSE_LEG_HEIGHT    15

#define LCDWIDTH            320
#define LCDHEIGHT           240

#define FALSE               0
#define TRUE                1

#define DRAW_MONSTERS_TICK  50

typedef struct pstruct {
    uint16_t x, y;
    uint8_t alive;
} sprite;

const sprite start_cannon = {(LCDWIDTH-CANNON_WIDTH)/2, LCDHEIGHT-CANNON_HEIGHT-1, 1};

volatile sprite monsters[MONSTERS_X][MONSTERS_Y];
volatile sprite cannon_laser;
volatile sprite cannon;
volatile sprite last_monsters[MONSTERS_X][MONSTERS_Y];
volatile sprite last_cannon_laser;
volatile sprite last_cannon;
sprite monster_lasers[MAX_MONSTER_LASERS];
sprite last_monster_lasers[MAX_MONSTER_LASERS];
//memory used for each sprite: 2B*2+1B = 5B
//total memory = (6 * 4 * 2 + 2 + 5 * 2) * 5B = (48 + 2 + 10) * 5B = 300B

uint16_t leftmost, rightmost, topmost, bottommost;
volatile int16_t left_o, top_o;
int16_t last_left_o, last_top_o;
volatile uint16_t score;
volatile uint8_t lives;
volatile uint8_t has_monsters;
volatile uint8_t lost_life;
//total memory = 21B
//TOTAL static = 321B ( plus defines ~= 350B)

static inline uint8_t intersect_sprite(sprite s1, uint8_t w1, uint8_t h1, 
                                       sprite s2, uint8_t w2, uint8_t h2);
uint8_t collision(sprite *monster);
void reset_lasers(void);
void draw_cannon(void);
void life_lost_sequence(void);
uint16_t rand(void);

ISR(TIMER1_COMPA_vect) {
    //stack space = 10B
    uint8_t x, y, l;
    uint8_t shoot, yinc;
    int8_t last_alive_monster_y;
    int8_t rotary;
    static int8_t xinc = MONSTER_SPEED;
    static uint16_t shot_p = 62000;
    static uint8_t monster_tick = 0;
    monster_tick = (monster_tick + 1) % DRAW_MONSTERS_TICK;
       
    //Cannon-Monster laser collision, and monster laser moving
    for(l = 0; l < MAX_MONSTER_LASERS; l++) {
        if(monster_lasers[l].alive) {
            monster_lasers[l].y += LASER_SPEED;
            if(monster_lasers[l].y >= LCDHEIGHT - LASER_HEIGHT) {
                monster_lasers[l].alive = FALSE;
            } else if(intersect_sprite(
                    cannon, CANNON_WIDTH, CANNON_HEIGHT,
                    monster_lasers[l], LASER_WIDTH, LASER_HEIGHT)) {
                lives--;
                lost_life = TRUE;              
                //Disable Timer1 interrupt.
                TIMSK1 &= ~_BV(OCIE1A);
                return;
            }
        }
    }
    
    //Move cannon lasers, and shoot
    shoot = get_switch_short(_BV(SWC)) || get_switch_rpt(_BV(SWC));
    if(cannon_laser.alive) {
        //Move lasers
        cannon_laser.y -= LASER_SPEED;
        if(cannon_laser.y <= LASER_SPEED) {
            cannon_laser.alive = FALSE;
        }
    } else if (shoot && !last_cannon_laser.alive) { //Ensure that 1 drawing cycle occurs before drawing again.
        //Cannon Shoot
        cannon_laser.x = cannon.x + (CANNON_WIDTH / 2) - LASER_WIDTH/2;
        cannon_laser.y = cannon.y - LASER_HEIGHT;
        cannon_laser.alive = TRUE;
        last_cannon_laser = cannon_laser;
    }
    
    //Monster-Cannon shot collision
    for(x = 0; x < MONSTERS_X; x++) {
        for(y = 0; y < MONSTERS_Y; y++) {
            if(monsters[x][y].alive == 1) {              
                //Collision
                if(collision(&monsters[x][y]))
                    continue;
            }
        }
    }
   
    //Monsters moving & shooting
    if(!monster_tick) {
        yinc = 0;
        if(leftmost < MONSTER_PADDING_X || rightmost > LCDWIDTH - MONSTER_PADDING_X) {
            xinc = -xinc;
            yinc = MONSTER_SPEED;
            shot_p -= 20;
        }
        has_monsters = 0;
        rightmost = bottommost = 0;
        leftmost = topmost = LCDWIDTH;
        last_alive_monster_y = -1;
        for(x = 0; x < MONSTERS_X; x++) {
            for(y = 0; y < MONSTERS_Y; y++) {
                if(monsters[x][y].alive == 1) {
                    last_alive_monster_y = y;
                    has_monsters = 1;
                    monsters[x][y].x += xinc;
                    monsters[x][y].y += yinc;
                    //Die when monsters get past cannon
                    if(monsters[x][y].y + MONSTER_HEIGHT >= cannon.y) {
                        lives = 0;
                        return;
                    }
                    has_monsters = 1;
                    //Update left/right/top/bottommost
                    if(monsters[x][y].x + MONSTER_WIDTH > rightmost)
                        rightmost = monsters[x][y].x + MONSTER_WIDTH;
                    if(monsters[x][y].x < leftmost)
                        leftmost = monsters[x][y].x;
                    if(monsters[x][y].y + MONSTER_HEIGHT > bottommost)
                        bottommost = monsters[x][y].y + MONSTER_HEIGHT;
                    if(monsters[x][y].y < topmost)
                        topmost = monsters[x][y].y;
                }
            }
            //Monster shoot
            if(last_alive_monster_y >= 0 && rand() > shot_p) {
                for(l = 0; l < MAX_MONSTER_LASERS; l++) {
                    if(!monster_lasers[l].alive && !last_monster_lasers[l].alive) {
                        monster_lasers[l].x = monsters[x][last_alive_monster_y].x + (MONSTER_WIDTH / 2);
                        monster_lasers[l].y = monsters[x][last_alive_monster_y].y + MONSTER_HEIGHT;
                        monster_lasers[l].alive = TRUE;
                        last_monster_lasers[l] = monster_lasers[l];
                        break;
                    }
                }
            }
        }
        left_o += xinc;
        top_o += yinc;
    }
    
    //Move cannon
    rotary = os_enc_delta();
    if(rotary < 0 && cannon.x > CANNON_SPEED) {
        cannon.x -= CANNON_SPEED;
    }
    else if (rotary > 0 && cannon.x + CANNON_WIDTH + CANNON_SPEED < LCDWIDTH) {
        cannon.x += CANNON_SPEED;
    }
}

ISR(INT6_vect) {
    //Stack size: 7B
    uint8_t x, y, l;
    
    //LIFE LOST
    if(lost_life) {
        life_lost_sequence();
        TIMSK1 |= _BV(OCIE1A);
        return;
    }
    
    //MOVE MONSTER LASERS
    for(l = 0; l < MAX_MONSTER_LASERS; l++) {
        if(monster_lasers[l].alive) {
            //Clear
            fill_rectangle_c(monster_lasers[l].x,
                             last_monster_lasers[l].y,
                             LASER_WIDTH,
                             monster_lasers[l].y - last_monster_lasers[l].y,
                             display.background);
            //Draw
            fill_rectangle_c(monster_lasers[l].x,
                             last_monster_lasers[l].y + LASER_HEIGHT,
                             LASER_WIDTH,
                             monster_lasers[l].y - last_monster_lasers[l].y,
                             RED);
        } else if(last_monster_lasers[l].alive) { //Has just died
            fill_rectangle_c(last_monster_lasers[l].x,
                           last_monster_lasers[l].y,
                           LASER_WIDTH, LASER_HEIGHT,
                           display.background);
        }
        last_monster_lasers[l] = monster_lasers[l];
    }
    
    //MOVE MONSTERS
    static uint8_t monster_drawing = 0;
    uint8_t right = left_o > last_left_o;
    uint8_t change_topmost = top_o - last_top_o;
    uint8_t change_leftmost = right ? left_o - last_left_o
                                    : last_left_o - left_o;
    for(x = 0; x < MONSTERS_X; x++) {
        for(y = 0; y < MONSTERS_Y; y++) {
            if(monsters[x][y].alive) { //Draw sprite                        
                //Vertical
                if(change_topmost) {
                    // Clear
                    // This -1 does not make sense.
                    fill_rectangle_c(last_monsters[x][y].x, last_monsters[x][y].y,
                        MONSTER_WIDTH, change_topmost - 1, display.background);
                }
                if(change_leftmost) {
                    //Horizontal clear
                    fill_rectangle_c(
                        right ? last_monsters[x][y].x
                              : monsters[x][y].x + MONSTER_WIDTH,
                        monsters[x][y].y,
                        change_leftmost,
                        MONSTER_HEIGHT,
                        display.background);
                    //Horizontal draw
                    if(monsters[x][y].alive > 1) {
                        continue;
                    }
                    if(monster_drawing) {
                        fill_image_pgm(monsters[x][y].x, monsters[x][y].y,
                            MONSTER_WIDTH, MONSTER_HEIGHT, monster_sprite_1);
                    } else {
                        fill_image_pgm(monsters[x][y].x, monsters[x][y].y,
                            MONSTER_WIDTH, MONSTER_HEIGHT, monster_sprite_2);
                    }
                }
                if(monsters[x][y].alive == 2 || monsters[x][y].alive == 3 || monsters[x][y].alive == 4 || monsters[x][y].alive == 5) { // Big explosion (4 frames)
                    fill_image_pgm(monsters[x][y].x, monsters[x][y].y,
                        MONSTER_WIDTH, MONSTER_HEIGHT, monster_sprite_3);
                    monsters[x][y].alive++;
                } else if(monsters[x][y].alive == 6 || monsters[x][y].alive == 7 || monsters[x][y].alive == 8) { // Small explosion (3 frames)
                    fill_image_pgm(monsters[x][y].x, monsters[x][y].y,
                        MONSTER_WIDTH, MONSTER_HEIGHT, monster_sprite_4);
                    monsters[x][y].alive++;
                } else if(monsters[x][y].alive == 9) { // Clear
                    fill_rectangle_c(monsters[x][y].x, monsters[x][y].y,
                        MONSTER_WIDTH, MONSTER_HEIGHT, display.background);
                    monsters[x][y].alive = FALSE;
                }
                last_monsters[x][y] = monsters[x][y];
            }
        }
    }
    if(change_topmost)
        last_top_o = top_o;
    if(change_leftmost) {
        last_left_o = left_o;
        monster_drawing ^= 1;
    }
    
    //MOVE LASER
    if(cannon_laser.alive) {
        //Clear
        fill_rectangle_c(cannon_laser.x,
                         cannon_laser.y + LASER_HEIGHT,
                         LASER_WIDTH, 
                         last_cannon_laser.y - cannon_laser.y,
                         display.background);
        //Draw
        fill_rectangle_c(cannon_laser.x,
                         cannon_laser.y,
                         LASER_WIDTH,
                         last_cannon_laser.y - cannon_laser.y,
                         BLUE);
    } else if(last_cannon_laser.alive) { //Has just died
        fill_rectangle_c(last_cannon_laser.x,
                       last_cannon_laser.y,
                       LASER_WIDTH, LASER_HEIGHT,
                       display.background);
    }
    last_cannon_laser = cannon_laser;
      
    //MOVE CANNON
    draw_cannon();
}

void draw_cannon() {
    fill_rectangle_c(last_cannon.x, last_cannon.y,
                   CANNON_WIDTH, CANNON_HEIGHT,
                   display.background);
    fill_image_pgm(cannon.x, cannon.y,
                   CANNON_WIDTH, CANNON_HEIGHT,
                   cannon_sprite);
    last_cannon = cannon;
}

ISR(TIMER3_COMPA_vect)
{
    scan_switches();
    scan_encoder();                
    // char buffer[4];
    // sprintf(buffer, "%04d", before - after);
    // display_string_xy(buffer, 10, 5);
	//char buffer[6];
	//sprintf(buffer, "%05d", rand());
	//display_string  (buffer);
	/*fps = 0;*/
}

void os_init(void) {
	/* 8MHz clock, no prescaling (DS, p. 48) */
    CLKPR = (1 << CLKPCE);
    CLKPR = 0;

	LED_INIT;
    init_lcd();
    init_encoder();
    
    /* Frame rate */
	set_frame_rate_hz(61); /* > 60 Hz  (KPZ 30.01.2015) */
    
	/* Enable tearing interrupt to get flicker free display */
	EIMSK |= _BV(INT6);
	/* Enable game timer interrupt (Timer 1 CTC Mode 4) */
	TCCR1A = 0;
    //CTC: TOP=OCR1A; don't use TOV1 (it's set at MAX=0xffff)
	TCCR1B = _BV(WGM12);
	TCCR1B |= _BV(CS11); // clk/8
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
    uint8_t x, y;
    do {
        for(x = 0; x < MONSTERS_X; x++) {
            for(y = 0; y < MONSTERS_Y; y++) {
                monsters[x][y].x = x*(MONSTER_WIDTH+MONSTER_PADDING_X)+MONSTER_PADDING_X;
                monsters[x][y].y = y*(MONSTER_HEIGHT+MONSTER_PADDING_Y)+MONSTER_TOP;
                monsters[x][y].alive = 1;                
                fill_image_pgm(
                    monsters[x][y].x,
                    monsters[x][y].y,
                    MONSTER_WIDTH,
                    MONSTER_HEIGHT,
                    monster_sprite_1);
                last_monsters[x][y] = monsters[x][y];
            }
        }
        has_monsters = 1;
        cannon = last_cannon = start_cannon;
        leftmost = left_o = last_left_o = MONSTER_PADDING_X;
        rightmost = MONSTERS_X*(MONSTER_WIDTH+MONSTER_PADDING_X);
        topmost = top_o = last_top_o = MONSTER_TOP;
        bottommost = MONSTERS_Y*(MONSTER_HEIGHT+MONSTER_PADDING_Y)+MONSTER_TOP-MONSTER_PADDING_Y;
        lives = 3;
        score = 0;
		OCR1A = 8192;
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
        reset_lasers();
        clear_screen();
    } while(1);
}

void reset_lasers(void) {
    uint8_t l;
    cannon_laser.alive = FALSE;
    for(l = 0; l < MAX_MONSTER_LASERS; l++) {
        monster_lasers[l].alive = FALSE;
    }
}

void life_lost_sequence(void) {
    uint8_t frames = 7;
    while(frames--) {
        fill_image_pgm(cannon.x, cannon.y,
                       CANNON_WIDTH, CANNON_HEIGHT,
                       cannon_sprite_2);
        _delay_ms(75);
        fill_image_pgm(cannon.x, cannon.y,
                       CANNON_WIDTH, CANNON_HEIGHT,
                       cannon_sprite_3);
        _delay_ms(75);
    }
    fill_rectangle_c(last_cannon.x, last_cannon.y,
                   CANNON_WIDTH, CANNON_HEIGHT,
                   display.background);
    fill_rectangle_c(cannon.x, cannon.y,
                   CANNON_WIDTH, CANNON_HEIGHT,
                   display.background);
    lost_life = FALSE;
    cannon = last_cannon = start_cannon;
    reset_lasers();
}

uint8_t collision(sprite *monster) {
    if(cannon_laser.alive && 
            intersect_sprite(cannon_laser, LASER_WIDTH, LASER_HEIGHT,
            *monster, MONSTER_WIDTH, MONSTER_HEIGHT)) {
        cannon_laser.alive = FALSE;
        monster->alive = 2;
        return TRUE;
    }
    return FALSE;
}

static inline uint8_t intersect_sprite(sprite s1, uint8_t w1, uint8_t h1, 
                                       sprite s2, uint8_t w2, uint8_t h2) {
    return !(s2.x > s1.x + w1
        || s2.x + w2 < s1.x
        || s2.y > s1.y + h1
        || s2.y + h2 < s1.y);
}

//http://en.wikipedia.org/wiki/Linear_feedback_shift_register
uint16_t rand() {
    static uint16_t lfsr = 0xACE1u;

    unsigned lsb = lfsr & 1;  /* Get lsb (i.e., the output bit). */
    lfsr >>= 1;               /* Shift register */
    if (lsb == 1)             /* Only apply toggle mask if output bit is 1. */
        lfsr ^= 0xB400u;        /* Apply toggle mask, value has 1 at bits corresponding
                             * to taps, 0 elsewhere. */
    return lfsr;
}