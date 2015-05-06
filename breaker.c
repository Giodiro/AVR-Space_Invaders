/*
 *
 * SPACE INVADERS FOR AVR90USB1286
 *
 */

#define NEED_IMAGES
 
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include "lcd.h"
#include "encoder.h"
#include "image.h"
#include "keyboard.h"

#define LED_INIT    DDRB  |=  _BV(PINB7)
#define LED_ON      PORTB |=  _BV(PINB7)
#define LED_OFF     PORTB &= ~_BV(PINB7) 

//Cannon
#define CANNON_WIDTH        26
#define CANNON_HEIGHT       10
#define CANNON_SPEED        5

//Lasers
#define LASER_WIDTH         1
#define LASER_HEIGHT        4
#define CANNON_LASER_SPEED  2
#define MONSTER_LASER_SPEED 1
#define MAX_MONSTER_LASERS  5

//Monsters
#define MONSTER_TOP         32
#define MONSTER_PADDING_X   10
#define MONSTER_PADDING_Y   3
#define MONSTER_WIDTH       26
#define MONSTER_HEIGHT      16
#define MONSTERS_X          5
#define MONSTERS_Y          5
#define MONSTER_SPEED       8
//MONSTERS_X * (MONSTER_PADDING_X + MONSTER_WIDTH) = 6*(30+8) = 228 < LCDWIDTH

#define MONSTER_POINTS      50
#define DRAW_MONSTERS_TICK  50

//Astro
#define ASTRO_WIDTH         32
#define ASTRO_HEIGHT        14
#define ASTRO_SPEED         1
#define ASTRO_POINTS        200
#ifdef ASTRO_DEBUG
    #define ASTRO_P             6
#else
    #define ASTRO_P         65519
#endif
#define ASTRO_Y             16

//Heart
#define HEART_WIDTH         8
#define HEART_HEIGHT        7

//House
#define HOUSE_WIDTH         31
#define HOUSE_HEIGHT        23
#define HOUSE_COUNT         4
#define HOUSE_PADDING_X     50
#define HOUSE_START_X       20
#define HOUSE_START_Y       180

#define LCDWIDTH            320
#define LCDHEIGHT           240

#define FALSE               0
#define TRUE                1

#define HOME_SCREEN_ITEMS   3
#define TRIANGLE_WIDTH      3
#define TRIANGLE_HEIGHT     6
#define HOME_SCREEN_X       100

#define STATE_HOME          0
#define STATE_PLAY          1
#define STATE_HIGH_SCORES   2
#define STATE_ABOUT         3
#define STATE_NEW_HIGH_SCORE 4

#define MAX_HIGH_SCORES     20
#define EEPROM_VALIDITY_CANARY  0xABCD
#define HIGH_SCORE_X        105

typedef struct {
    uint16_t x, y;
    uint8_t alive;
    uint8_t kind;
} sprite;

const sprite start_cannon = {(LCDWIDTH-CANNON_WIDTH)/2, LCDHEIGHT-CANNON_HEIGHT-1, 1, 0};
const uint8_t start_house_data[24] = {
    0xFF,0xFC,      //11111111111111111111111111110000    
    0xFF,0xFC,      //11111111111111111111111111110000    
    0xFF,0xFC,      //11111111111111111111111111110000    
    0xF8,0x7C,      //11111111110000000011111111110000    
    0xF0,0x3C,      //11111111000000000000111111110000    
    0xE0,0x1C,      //11111100000000000000001111110000    
    0xE0,0x1C,      //11111100000000000000001111110000    
    0xE0,0x1C,      //11111100000000000000001111110000    
    0xE0,0x1C,      //11111100000000000000001111110000    
    0xE0,0x1C,      //11111100000000000000001111110000    
    0xE0,0x1C,      //11111100000000000000001111110000    
    0x00,0x00,      //00000000000000000000000000000000    
};

volatile sprite monsters[MONSTERS_X][MONSTERS_Y];
volatile sprite cannon_laser;
volatile sprite cannon;
volatile sprite last_monsters[MONSTERS_X][MONSTERS_Y];
volatile sprite last_cannon_laser;
volatile sprite last_cannon;
volatile sprite houses[HOUSE_COUNT];
volatile sprite astro;
volatile sprite last_astro;
sprite monster_lasers[MAX_MONSTER_LASERS];
sprite last_monster_lasers[MAX_MONSTER_LASERS];
uint8_t house_data[HOUSE_COUNT][24];
uint8_t old_house_data[HOUSE_COUNT][24];
uint16_t EEMEM eeprom_high_scores[MAX_HIGH_SCORES + 1];
uint16_t high_scores[MAX_HIGH_SCORES] = {0,1};
//memory used for each sprite: 2B*2+1B = 5B
//total memory = (6 * 4 * 2 + 2 + 5 * 2) * 5B = (48 + 2 + 10) * 5B = 300B

uint16_t leftmost, rightmost, topmost, bottommost;
volatile int16_t left_o, top_o;
int16_t last_left_o, last_top_o;
volatile uint16_t score;
volatile uint8_t lives;
volatile uint8_t has_monsters;
volatile uint8_t lost_life;
uint16_t random_seed;
//total memory = 21B
//TOTAL static = 321B ( plus defines ~= 350B)

//Home screen stuff
volatile uint8_t selected_item;
volatile uint8_t last_selected_item;
volatile uint8_t game_state;

//High score stuff
uint8_t is_highscore_drawn;

//New High score stuff
uint8_t is_new_high_drawn;

static inline uint8_t intersect_sprite(sprite s1, uint8_t w1, uint8_t h1, 
                                       sprite s2, uint8_t w2, uint8_t h2);
uint8_t collision(volatile sprite *monster);
void reset_sprites(void);
void draw_cannon(void);
void draw_monsters(void);
void draw_monster_lasers(void);
void draw_lasers(void);
void draw_score(void);
void draw_lives(void);
void draw_astro(void);
void draw_houses(void);
void life_lost_sequence(void);
void in_game_movement(void);
void home_screen_movement(void);
void draw_home_screen(void);
void draw_high_scores(void);
void draw_new_high_score(void);
void new_high_score_movement(void);
void high_score_movement(void);
void load_high_scores(void);
void store_high_scores(void);
uint8_t save_high_score(uint16_t score);

void draw_monster(volatile sprite *monster, uint8_t version);
uint8_t intersect_pp(sprite s1, uint8_t w1, uint8_t h1,
                     sprite s2, uint8_t w2, uint8_t h2,
                     uint8_t *data, rectangle *result);
uint16_t rand_init(void);
uint16_t rand(void);

ISR(TIMER1_COMPA_vect) {
    switch(game_state) {
        case STATE_HOME:
            home_screen_movement();
            break;
        case STATE_PLAY:
            in_game_movement();
            break;
        case STATE_HIGH_SCORES:
            high_score_movement();
            break;
        case STATE_NEW_HIGH_SCORE:
            new_high_score_movement();
            break;
    }
}

ISR(INT6_vect) {
    switch(game_state) {
        case STATE_HOME:
            draw_home_screen();
            break;
        case STATE_PLAY:
            draw_score();
            
            //LIFE LOST
            if(lost_life) {
                life_lost_sequence();
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
            
            //DRAW ASTRO
            draw_astro();
            
            //HOUSES
            draw_houses();
            break;
        case STATE_HIGH_SCORES:
            draw_high_scores();
            break;
        case STATE_NEW_HIGH_SCORE:
            draw_new_high_score();
            break;
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

void draw_astro(void) {
    if(astro.alive) {
        if(last_astro.x != astro.x && astro.alive <= 2) {
            //Clear
            fill_rectangle_c(last_astro.x, astro.y,
                             astro.x - last_astro.x,
                             ASTRO_HEIGHT, display.background);
            if(astro.alive == 1) {
                //Fill
                fill_image_pgm_2b(astro.x, astro.y, ASTRO_WIDTH, ASTRO_HEIGHT, astro_sprite);
            }
        }
        if(astro.alive >= 2 && astro.alive <= 9) {
            if(astro.alive == 2) {
                fill_image_pgm_2b(astro.x, astro.y,
                    MONSTER_WIDTH, MONSTER_HEIGHT, monster_sprite_exp);
                fill_rectangle_c(astro.x + MONSTER_WIDTH, astro.y, 
                    ASTRO_WIDTH - MONSTER_WIDTH, ASTRO_HEIGHT, display.background);
            }
            astro.alive++;
        } else if(astro.alive >= 10) {
            fill_rectangle_c(astro.x, astro.y, MONSTER_WIDTH, MONSTER_HEIGHT, display.background);
            astro.alive = FALSE;
        }
        last_astro = astro;
    } else if(last_astro.alive) {
        fill_rectangle_c(last_astro.x, astro.y, ASTRO_WIDTH, ASTRO_HEIGHT, display.background);
        last_astro = astro;
    }
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
                if(change_leftmost && monsters[x][y].alive <= 2) {
                    //Horizontal clear
                    fill_rectangle_c(
                        right ? last_monsters[x][y].x
                              : monsters[x][y].x + MONSTER_WIDTH,
                        monsters[x][y].y,
                        change_leftmost,
                        MONSTER_HEIGHT,
                        display.background);
                    //Horizontal draw
                    if(monsters[x][y].alive == 1) {
                        draw_monster(&monsters[x][y], monster_drawing);
                    }
                }
                if(monsters[x][y].alive >= 2 && monsters[x][y].alive <= 9) { // Big explosion (4 frames)
                    if(monsters[x][y].alive == 2) {
                        fill_image_pgm_2b(monsters[x][y].x, monsters[x][y].y,
                            MONSTER_WIDTH, MONSTER_HEIGHT, monster_sprite_exp);
                    }
                    monsters[x][y].alive++;
                } else if(monsters[x][y].alive >= 10) { // Clear
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
    uint8_t l, h;
    for(l = 0; l < MAX_MONSTER_LASERS; l++) {
        if(monster_lasers[l].alive) {
            h = (monster_lasers[l].y - last_monster_lasers[l].y) > LASER_HEIGHT;
            //Clear
            fill_rectangle_c(monster_lasers[l].x,
                             last_monster_lasers[l].y,
                             LASER_WIDTH,
                             h ? LASER_HEIGHT : monster_lasers[l].y - last_monster_lasers[l].y,
                             display.background);
            //Draw
            fill_rectangle_c(monster_lasers[l].x,
                             h ? monster_lasers[l].y : last_monster_lasers[l].y + LASER_HEIGHT,
                             LASER_WIDTH,
                             h ? LASER_HEIGHT : monster_lasers[l].y - last_monster_lasers[l].y,
                             RED);
            last_monster_lasers[l] = monster_lasers[l];
        } else if(last_monster_lasers[l].alive) { //Has just died
            fill_rectangle_c(last_monster_lasers[l].x,
                           last_monster_lasers[l].y,
                           LASER_WIDTH, LASER_HEIGHT,
                           display.background);
            last_monster_lasers[l] = monster_lasers[l];
        }
    }
}

void draw_lasers(void) {
    if(cannon_laser.alive) {
        //Clear
        uint8_t h = (last_cannon_laser.y - cannon_laser.y) > LASER_HEIGHT;
        fill_rectangle_c(cannon_laser.x,
                         h ? last_cannon_laser.y : cannon_laser.y + LASER_HEIGHT,
                         LASER_WIDTH, 
                         h ? LASER_HEIGHT : last_cannon_laser.y - cannon_laser.y,
                         display.background);
        //Draw
        fill_rectangle_c(cannon_laser.x,
                         cannon_laser.y,
                         LASER_WIDTH,
                         h ? LASER_HEIGHT : last_cannon_laser.y - cannon_laser.y,
                         BLUE);
        last_cannon_laser = cannon_laser;
    } else if(last_cannon_laser.alive) { //Has just died
        fill_rectangle_c(last_cannon_laser.x,
                       last_cannon_laser.y,
                       LASER_WIDTH, LASER_HEIGHT,
                       display.background);
        last_cannon_laser = cannon_laser;
    }
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
    //Clear the switches to prevent random firing as soon as game restarts.
    get_switch_rpt(_BV(SWC)); 
    get_switch_short(_BV(SWC));
    reset_sprites();
    TIMSK1 |= _BV(OCIE1A);
}

void draw_houses(void) {
    uint8_t x, y, h;
    for(h = 0; h < HOUSE_COUNT; h++) {
        for(y = 0; y < HOUSE_HEIGHT >> 1; y++) {
            for(x = 0; x < HOUSE_WIDTH >> 1; x++) {
                // !(house_1[y/2+x/8] & (128 >> x%8))
                uint8_t index = (y<<1) + (x>>3);
                uint8_t offset = 128 >> (x & 0x07);
                if((house_data[h][index] & offset) != (old_house_data[h][index] & offset)) {
                    fill_rectangle_c(houses[h].x + (x<<1),
                                     houses[h].y + (y<<1), 2, 2, BLACK);
                    old_house_data[h][index] = house_data[h][index];
                }
            }
        }
    }
}

void draw_monster(volatile sprite *monster, uint8_t version) {
    if(monster->kind == 0) {
        if(version == 0) {
            fill_image_pgm_2b(
                monster->x, monster->y,
                MONSTER_WIDTH, MONSTER_HEIGHT,
                monster_sprite_1A);
        } else {
            fill_image_pgm_2b(
                monster->x, monster->y,
                MONSTER_WIDTH, MONSTER_HEIGHT,
                monster_sprite_1B);
        }
    } else if(monster->kind == 1) {
        if(version == 0) {
            fill_image_pgm_2b(
                monster->x, monster->y,
                MONSTER_WIDTH, MONSTER_HEIGHT,
                monster_sprite_2A);
        } else {
            fill_image_pgm_2b(
                monster->x, monster->y,
                MONSTER_WIDTH, MONSTER_HEIGHT,
                monster_sprite_2B);
        }
    } else if(monster->kind == 2) {
        if(version == 0) {
            fill_image_pgm_2b(
                monster->x, monster->y,
                MONSTER_WIDTH, MONSTER_HEIGHT,
                monster_sprite_3A);
        } else {
            fill_image_pgm_2b(
                monster->x, monster->y,
                MONSTER_WIDTH, MONSTER_HEIGHT,
                monster_sprite_3B);
        }
    }
}

void in_game_movement(void) {
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
            monster_lasers[l].y += MONSTER_LASER_SPEED;
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
                            houses[x], HOUSE_WIDTH, HOUSE_HEIGHT, house_data[x], &r)) {
                            uint16_t tempx = (r.left - houses[x].x) >> 1;
                            uint16_t tempy = (r.bottom - houses[x].y) >> 1;
                            house_data[x][(tempy << 1) + (tempx>>3)] &= ~(128 >> (tempx & 0x07));
                            monster_lasers[l].alive = FALSE;
                        }
                    }
                }
            }
        }
    }
    
    //Move cannon lasers, and shoot
    shoot = get_switch_short(_BV(SWC)) | get_switch_rpt(_BV(SWC));
    if(cannon_laser.alive) {
        //Move lasers
        cannon_laser.y -= CANNON_LASER_SPEED;
        if(cannon_laser.y <= ASTRO_Y) { //Reached top of screen (avoid going over score/lives)
            cannon_laser.alive = FALSE;
        }
        
        //House - cannon shot collision
        for(x = 0; x < HOUSE_COUNT; x++) {
            if (intersect_sprite(houses[x], HOUSE_WIDTH, HOUSE_HEIGHT,
                                cannon_laser, LASER_WIDTH, LASER_HEIGHT)) {
                 if(intersect_pp(cannon_laser, LASER_WIDTH, LASER_HEIGHT,
                                houses[x], HOUSE_WIDTH, HOUSE_HEIGHT, house_data[x], &r)) {
                    uint16_t tempx = ((r.left - houses[x].x) >> 1);
                    uint16_t tempy = ((r.top - houses[x].y) >> 1);
                    house_data[x][(tempy << 1) + (tempx>>3)] &= ~(128 >> (tempx & 0x07));
                    cannon_laser.alive = FALSE;
                }
            }
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
        
        for(x = 0; x < MONSTERS_X; x++) {
            last_alive_monster_y = -1;
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
    
    //Astro creation (and moving/collision)
    if(astro.alive == 1) {
        if(cannon_laser.alive && 
                intersect_sprite(cannon_laser, LASER_WIDTH, LASER_HEIGHT,
                astro, ASTRO_WIDTH, ASTRO_HEIGHT)) {
            cannon_laser.alive = FALSE;
            astro.alive = 2;
            score += ASTRO_POINTS;
        } else {
            astro.x += ASTRO_SPEED;
            if(astro.x + ASTRO_WIDTH >= LCDWIDTH) {
                astro.alive = FALSE;
            }
        }
    }
    else if(!last_astro.alive && rand() > ASTRO_P) {
        astro.alive = TRUE;
        astro.x = 0;
        astro.y = ASTRO_Y;
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

void home_screen_movement(void) {
    static uint8_t tick = 9;
    if(tick == 10) {
        int8_t rotary = os_enc_delta();
        if(rotary < 0 && selected_item > 0)
            selected_item--;
        else if(rotary > 0)
            selected_item = (selected_item + 1) % HOME_SCREEN_ITEMS;
        tick = 0;
    }
    tick++;
    
    if(get_switch_short(_BV(SWC))) {
        clear_switches();
        switch(selected_item) {
            case 0:
                game_state = STATE_PLAY;
                break;
            case 1: 
                clear_screen();
                load_high_scores();
                is_highscore_drawn = FALSE;
                game_state = STATE_HIGH_SCORES;
                break;
            case 2: 
                game_state = STATE_ABOUT;
                break;
        }
    }
}

void high_score_movement(void) {
    if(get_switch_short(_BV(SWW))) {
        clear_screen();
        last_selected_item = 5;
        selected_item = 1;
        clear_switches();
        game_state = STATE_HOME;
    }
}

void new_high_score_movement(void) {
    if(move_keyboard()) { //Enter pressed
        clear_screen();
        last_selected_item = 5;
        selected_item = 1;
        //No need to clear switches (keyboard handles it)
        game_state = STATE_HOME;
    }
}

void draw_home_screen(void) {
    //character width = 10
    uint8_t triangle_y;
    if(last_selected_item == selected_item)
        return;
    clear_screen();
    
    display_string_xy_col("Play!", HIGH_SCORE_X, 90, selected_item == 0 ? BLUE : WHITE);
    display_string_xy_col("High scores", HIGH_SCORE_X, 115, selected_item == 1 ? BLUE : WHITE);
    display_string_xy_col("About", HIGH_SCORE_X, 140, selected_item == 2 ? BLUE : WHITE);
    
    switch(selected_item) {
        case 0: triangle_y = 90;
                break;
        case 1: triangle_y = 115;
                break;
        case 2: triangle_y = 140;
                break;
        default: return;
    }
    fill_image_pgm(HIGH_SCORE_X - TRIANGLE_WIDTH * 2, triangle_y, TRIANGLE_WIDTH, TRIANGLE_HEIGHT, triangle_sprite);
    last_selected_item = selected_item;
}

void draw_high_scores(void) {
    uint8_t i, h;
    char buff[4];
    
    if(is_highscore_drawn)
        return;
    display_string_xy("HIGH SCORES", 105, 5);
    //Assumes MAX_HIGH_SCORES >= 3
    h = 20;
    display_string_xy_col(" 1.    ", HIGH_SCORE_X, h, GOLD);
    sprintf(buff, "%04d", high_scores[0]);
    display_string_col(buff, GOLD);
    h+=10;
    display_string_xy_col(" 2.    ", HIGH_SCORE_X, h, SILVER);
    sprintf(buff, "%04d", high_scores[1]);
    display_string_col(buff, SILVER);
    h+=10;
    display_string_xy_col(" 3.    ", HIGH_SCORE_X, h, TAN);
    sprintf(buff, "%04d", high_scores[2]);
    display_string_col(buff, TAN);
    h += 10;
    for(i = 3; i < MAX_HIGH_SCORES; i++, h+=10) {
        sprintf(buff, "%2d", i+1);
        display_string_xy_col(buff, HIGH_SCORE_X, h, WHITE);
        display_string_col(".    ", WHITE);
        sprintf(buff, "%04d", high_scores[i]);
        display_string_col(buff, WHITE);
    }
    is_highscore_drawn = TRUE;
}

void draw_new_high_score(void) {
    draw_keyboard();
    if(is_new_high_drawn)
        return;
    
    display_string_xy("New High Score!!!", 65, 20);
    display_string_xy("Enter your name:", 0, 50);
    is_new_high_drawn = TRUE;
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
	set_frame_rate_hz(31); /* > 60 Hz  (KPZ 30.01.2015) */
    
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
    
    random_seed = rand_init();
}

int main() {
    os_init();
    uint8_t x, y, h;
    do {
        game_state = STATE_HOME;
        last_selected_item = 5;
		OCR1A = 6192;
        //Home screen loop
        sei();
        while(game_state != STATE_PLAY);
        cli();
        
        reset_sprites();
        clear_screen();
        //Game loop
        for(x = 0; x < MONSTERS_X; x++) {
            for(y = 0; y < MONSTERS_Y; y++) {
                monsters[x][y].x = x*(MONSTER_WIDTH+MONSTER_PADDING_X)+MONSTER_PADDING_X;
                monsters[x][y].y = y*(MONSTER_HEIGHT+MONSTER_PADDING_Y)+MONSTER_TOP;
                monsters[x][y].alive = 1;
                monsters[x][y].kind = y >> 1;
                draw_monster(&monsters[x][y],0);
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
        draw_lives();
        LED_ON;
        sei();
        while(lives && has_monsters);
        cli();
        LED_OFF;
        if(!lives) {
            life_lost_sequence();
            display_string_xy("Game Over", 90, 150);
            clear_screen();
            PORTB |= _BV(PB6);
            while(!get_switch_short(_BV(SWC))) {
                if(PINB % _BV(PB6))
                    LED_ON;
                else
                    LED_OFF;
                scan_switches();
            }
        } else {
            // display_string_xy("YOU WIN!", 90, 150);
            if(save_high_score(score)) {
                clear_screen();
                store_high_scores();
                game_state = STATE_NEW_HIGH_SCORE;
                is_new_high_drawn = FALSE;
                init_keyboard();
                sei();
                while(game_state == STATE_NEW_HIGH_SCORE);
                cli();
            }
        }
        reset_sprites();
    } while(1);
}

void reset_sprites(void) {
    uint8_t l;
    cannon_laser.alive = FALSE;
    for(l = 0; l < MAX_MONSTER_LASERS; l++) {
        monster_lasers[l].alive = FALSE;
    }
    astro.alive = FALSE;
}

uint8_t collision(volatile sprite *monster) {
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
                     uint8_t *data, rectangle *result) {
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
    tempy = ((top - s2.y) >> 1);
    for(y = top; y <= bottom; y+=2, tempy++) {
        for(x = left, tempx = ((left - s2.x) >> 1) ; x <= right; x+=2, tempx++) {
            //Since laser is never transparent, only need to check if
            //house (data) is opaque.
            uint8_t index = (tempy<<1) + (tempx>>3);
            if(data[index] & (128 >> (tempx & 0x07))) {
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

void load_high_scores(void) {
    //Already loaded (high_scores is initialized with [0] = 0; [1] = 1;)
    if(high_scores[0] >= high_scores[1])
        return;
    uint8_t i;
    uint16_t canary = eeprom_read_word(&eeprom_high_scores[MAX_HIGH_SCORES]);
    
    for(i = 0; i < MAX_HIGH_SCORES; i++) {
        if(canary != EEPROM_VALIDITY_CANARY) {
            high_scores[i] = 0;
        } else {
            high_scores[i] = eeprom_read_word(&eeprom_high_scores[i]);
        }
    }
}

void store_high_scores(void) {
    uint8_t i;
    for(i = 0; i < MAX_HIGH_SCORES; i++) {
        eeprom_update_word(&eeprom_high_scores[i], high_scores[i]);
    }
    eeprom_update_word(&eeprom_high_scores[MAX_HIGH_SCORES], EEPROM_VALIDITY_CANARY);
}

//Returns true if the score goes into high scores.
uint8_t save_high_score(uint16_t score) {
    uint8_t i = MAX_HIGH_SCORES - 1;
    if(score < high_scores[i])
        return FALSE;
    while(i > 0 && score > high_scores[i-1])
    {
        high_scores[i] = high_scores[i-1];
        i--;
    }
    high_scores[i] = score;
    return TRUE;
}

uint16_t rand_init(void) {
    ADMUX |= _BV(REFS0) | _BV(MUX3) | _BV(MUX1) | _BV(MUX0);
    //Prescaler
    ADCSRA |= _BV(ADPS2) | _BV(ADPS1);
    //Enable and start
    ADCSRA |= _BV(ADEN) | _BV(ADSC);
    //Wait until complete
    while(! (ADCSRA & _BV(ADIF))) {
        _delay_ms(2);
    }
    //Read result
    uint16_t res = ADCL;
    res |= ADCH << 8;
    //Disable
    ADCSRA &= ~_BV(ADEN);
    return res;
}

//http://en.wikipedia.org/wiki/Linear_feedback_shift_register
uint16_t rand(void) {
    // static uint16_t random_seed = 0xACE1u;

    unsigned lsb = random_seed & 1;  /* Get lsb (i.e., the output bit). */
    random_seed >>= 1;               /* Shift register */
    if (lsb == 1)             /* Only apply toggle mask if output bit is 1. */
        random_seed ^= 0xB400u;        /* Apply toggle mask, value has 1 at bits corresponding
                             * to taps, 0 elsewhere. */
    return random_seed;
}