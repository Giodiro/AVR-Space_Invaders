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

//Cannon
#define CANNON_WIDTH        26
#define CANNON_HEIGHT       10
#define CANNON_SPEED        4

//Lasers
#define LASER_WIDTH         1
#define LASER_HEIGHT        4
#define LASER_SPEED         1
#define MAX_MONSTER_LASERS  5

//Monsters
#define MONSTER_TOP         20
#define MONSTER_PADDING_X   10
#define MONSTER_PADDING_Y   9

#define MONSTER_WIDTH       22
#define MONSTER_HEIGHT      16
#define MONSTERS_X          6
#define MONSTERS_Y          4
#define MONSTER_SPEED       8
//MONSTERS_X * (MONSTER_PADDING_X + MONSTER_WIDTH) = 6*(30+8) = 228 < LCDWIDTH

#define MONSTER_POINTS      50
#define DRAW_MONSTERS_TICK  50

//Heart
#define HEART_WIDTH         8
#define HEART_HEIGHT        7

//House
#define HOUSE_WIDTH         32
#define HOUSE_HEIGHT        24
#define HOUSE_COUNT         4
#define HOUSE_PADDING_X     50
#define HOUSE_START_X       20
#define HOUSE_START_Y       180

#define LCDWIDTH            320
#define LCDHEIGHT           240

#define FALSE               0
#define TRUE                1

typedef struct {
    uint16_t x, y;
    uint8_t alive;
} sprite;

const sprite start_cannon = {(LCDWIDTH-CANNON_WIDTH)/2, LCDHEIGHT-CANNON_HEIGHT-1, 1};
const uint8_t start_house_data[24] = {
    0xFF,0xFF,          // 11111111111111111111111111111111
    0xFF,0xFF,          // 11111111111111111111111111111111
    0xFF,0xFF,          // 11111111111111111111111111111111
    0xFC,0x3F,          // 11111111111100000000111111111111
    0xF8,0x1F,          // 11111111110000000000001111111111
    0xF0,0x0F,          // 11111111000000000000000011111111
    0xF0,0x0F,          // 11111111000000000000000011111111
    0xF0,0x0F,          // 11111111000000000000000011111111
    0xF0,0x0F,          // 11111111000000000000000011111111
    0xF0,0x0F,          // 11111111000000000000000011111111
    0xF0,0x0F,          // 11111111000000000000000011111111
    0xF0,0x0F,          // 11111111000000000000000011111111
};

volatile sprite monsters[MONSTERS_X][MONSTERS_Y];
volatile sprite cannon_laser;
volatile sprite cannon;
volatile sprite last_monsters[MONSTERS_X][MONSTERS_Y];
volatile sprite last_cannon_laser;
volatile sprite last_cannon;
volatile sprite houses[HOUSE_COUNT];
sprite monster_lasers[MAX_MONSTER_LASERS];
sprite last_monster_lasers[MAX_MONSTER_LASERS];
uint8_t house_data[HOUSE_COUNT][24];
uint8_t old_house_data[HOUSE_COUNT][24];
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
void draw_monsters(void);
void draw_monster_lasers(void);
void draw_lasers(void);
void draw_score(void);
void draw_lives(void);
void life_lost_sequence(void);
uint8_t intersect_pp(sprite s1, uint8_t w1, uint8_t h1,
                     sprite s2, uint8_t w2, uint8_t h2,
                     uint8_t *data, rectangle *result,
                     uint8_t data_x_offset, uint8_t data_y_offset);
uint16_t rand(void);

ISR(TIMER1_COMPA_vect) {
    //stack space = 10B
    uint8_t x, y, l;
    uint8_t shoot, yinc;
    int8_t last_alive_monster_y;
    int8_t rotary;
    rectangle r;
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
            } else if(intersect_sprite(cannon, CANNON_WIDTH, CANNON_HEIGHT,
                        monster_lasers[l], LASER_WIDTH, LASER_HEIGHT)) { //Colision with cannon
                lives--;
                lost_life = TRUE;              
                //Disable Timer1 interrupt.
                TIMSK1 &= ~_BV(OCIE1A);
                return;
            } else {
                for(x = 0; x < HOUSE_COUNT; x++) {
                    if (intersect_sprite(houses[x], HOUSE_WIDTH, HOUSE_HEIGHT,
                            monster_lasers[l], LASER_WIDTH, LASER_HEIGHT)) { //Collision with houses
                        //In the external if, a bounding box collision check is performed.
                        //In the internal if, a pixel perfect collision check is needed.
                        if(intersect_pp(monster_lasers[l], LASER_WIDTH, LASER_HEIGHT,
                            houses[x], HOUSE_WIDTH, HOUSE_HEIGHT, house_data[x], &r, houses[x].x, houses[x].y)) {
                            uint16_t tempx = (r.left - houses[x].x) >> 1;
                            uint16_t tempy = (r.top - houses[x].y) >> 1;
                            house_data[x][(tempy << 1) + (tempx>>3)] &= ~(128 >> (tempx & 0x07));
                            monster_lasers[l].alive = FALSE;
                        }
                    }
                }
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
                if(collision(&monsters[x][y])) {
                    score += MONSTER_POINTS;
                    continue;
                }
            }
        }
    }
    
    //House - cannon shot collision
    for(x = 0; x < HOUSE_COUNT; x++) {
        if (intersect_sprite(houses[x], HOUSE_WIDTH, HOUSE_HEIGHT,
                            cannon_laser, LASER_WIDTH, LASER_HEIGHT)) {
             if(intersect_pp(cannon_laser, LASER_WIDTH, LASER_HEIGHT,
                            houses[x], HOUSE_WIDTH, HOUSE_HEIGHT, house_data[x], &r, houses[x].x, houses[x].y)) {
                uint16_t tempx = (r.left - houses[x].x) >> 1;
                uint16_t tempy = (r.top - houses[x].y) >> 1;
                house_data[x][(tempy << 1) + (tempx>>3)] &= ~(128 >> (tempx & 0x07));
                cannon_laser.alive = FALSE;
            }
        }
    }
   
    //Monsters moving & shooting
    if(!monster_tick) {
        yinc = 0;
        if(leftmost < MONSTER_PADDING_X || rightmost > LCDWIDTH - MONSTER_PADDING_X) {
            xinc = -xinc;
            yinc = MONSTER_SPEED;
            shot_p -= 50;
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
                        monster_lasers[l].y = monsters[x][last_alive_monster_y].y + MONSTER_HEIGHT + 1;
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
    draw_score();
    
    //LIFE LOST
    if(lost_life) {
        life_lost_sequence();
        TIMSK1 |= _BV(OCIE1A);
        return;
    }
    
    //MOVE MONSTER LASERS
    draw_monster_lasers();
    
    //MOVE MONSTERS
    draw_monsters();
    
    //MOVE LASER
    draw_lasers();
      
    //MOVE CANNON
    draw_cannon();
    
    //HOUSES
    uint8_t x, y, h;
    for(h = 0; h < HOUSE_COUNT; h++) {
        for(y = 0; y < HOUSE_HEIGHT >> 1; y++) {
            for(x = 0; x < HOUSE_WIDTH >> 1; x++) {
                // !(house_1[y/2+x/8] & (128 >> x%8))
                uint8_t index = (y<<1) + (x>>3);
                uint8_t offset = 128 >> (x & 0x07);
                if((house_data[h][index] & offset) != (old_house_data[h][index] & offset)) {
                    fill_rectangle_c(houses[h].x + (x<<1), houses[h].y + (y<<1), 2, 2, BLACK);
                    old_house_data[h][index] = house_data[h][index];
                }
            }
        }
    }
}

void draw_cannon(void) {
    fill_rectangle_c(last_cannon.x, last_cannon.y,
                   CANNON_WIDTH, CANNON_HEIGHT,
                   display.background);
    fill_image_pgm(cannon.x, cannon.y,
                   CANNON_WIDTH, CANNON_HEIGHT,
                   cannon_sprite);
    last_cannon = cannon;
}

void draw_monsters(void) {
    uint8_t x, y;
    //Flag to indicate whether to use monster_sprite_1 or 2.
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
}

void draw_monster_lasers(void) {
    uint8_t l;
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
}

void draw_lasers(void) {
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
}

void draw_score(void) {
    char buffer[4];
    sprintf(buffer, "%04d", score);
    display_string_xy(buffer, 250, 5);
}

void draw_lives(void) {
    uint16_t heart_offset;
    uint8_t i;
    for(i = 0, heart_offset = 280; i < lives; i++, heart_offset += 13) {
        fill_image_pgm(heart_offset, 5, HEART_WIDTH, HEART_HEIGHT, heart_sprite);
    }
    for(;i < 3; i++, heart_offset += 13) {
        fill_rectangle_c(heart_offset, 5, HEART_WIDTH, HEART_HEIGHT, display.background);
    }
}

void life_lost_sequence(void) {
    uint8_t frames = 7;
    draw_lives();
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

ISR(TIMER3_COMPA_vect)
{
    scan_switches();
    scan_encoder();           
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
    uint8_t x, y, h;
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
        
        for(h = 0; h < HOUSE_COUNT; h++) {
            for(x = 0; x < 24; x++) {
                //TODO: use memcpy?
                house_data[h][x] = start_house_data[x];
                old_house_data[h][x] = start_house_data[x];
            }
            houses[h].alive = TRUE;
            houses[h].x = HOUSE_START_X + (HOUSE_WIDTH + HOUSE_PADDING_X) * h;
            houses[h].y = HOUSE_START_Y;
            for(y = 0; y < HOUSE_HEIGHT / 2; y++) {
                for(x = 0; x < HOUSE_WIDTH / 2; x++) {
                    if(house_data[h][(y<<1) + (x>>3)] & (128 >> (x & 0x07))) {
                        fill_rectangle_c(houses[h].x + (x<<1),
                                         houses[h].y + (y<<1), 2, 2, LIME_GREEN);
                    }
                }
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
        draw_lives();
        draw_score();
        LED_ON;
        sei();
        while(lives && has_monsters);
        cli();
        LED_OFF;
        if(!lives) {
            life_lost_sequence();
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

uint8_t intersect_pp(sprite s1, uint8_t w1, uint8_t h1,
                     sprite s2, uint8_t w2, uint8_t h2,
                     uint8_t *data, rectangle *result,
                     uint8_t data_x_offset, uint8_t data_y_offset) {
    uint16_t left, right, top, bottom;
    uint16_t x, y;
    uint16_t tempx, tempy;
    if(s1.y > s2.y) top = s1.y;
    else top = s2.y;
    if(s1.y+h1 > s2.y+h2) bottom = s2.y+h2;
    else bottom = s1.y+h1;
    if(s1.x > s2.x) left = s1.x;
    else left = s2.x;
    if(s1.x+w1 > s2.x+w2) right = s2.x+w2;
    else right = s1.x+w1;
    
    //The divisions by 2 are because one bit in data represents 2 pixels.
    tempy = (top - data_x_offset) >> 1;
    for(y = top; y <= bottom; y+=2, tempy ++) {
        for(x = left, tempx = (left - data_y_offset) >> 1; x <= right; x+=2, tempx++) {
            //Since laser is never transparent, only need to check if
            //house (data) is opaque.
            uint8_t index = (tempy<<1) + (tempx>>3);
            if(data[index] & (128 >> (x & 0x07))) {
                result->left = left;
                result->right = right;
                result->top = top;
                result->bottom = bottom;
                return 1;
            }
        }
    }
    return 0;
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