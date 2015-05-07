/* encoder.h

   |      |     | Signal on |                       |
   | Port | Pin | Schematic | Function              |
   |------+-----+-----------+-----------------------|
   | E    |   4 | ROTA      | Rotary Encoder A      |
   | E    |   5 | ROTB      | Rotary Encoder B      |
   | E    |   7 | SWC       | Switch wheel "Centre" |
   |------+-----+-----------+-----------------------|
   | C    |   2 | SWN       | Switch wheel "North"  |
   | C    |   3 | SWE       | Switch wheel "East"   |
   | C    |   4 | SWS       | Switch wheel "South"  |
   | C    |   5 | SWW       | Switch wheel "West"   |
   |------+-----+-----------+-----------------------|
   | B    |   6 | CD        | SD Card Detetcion     |
 
*/

#ifndef ENCODER_H
#define ENCODER_H
#include <stdint.h>

#define SWN     PC2 /* North direction button */
#define SWE     PC3 /* East direction button */
#define SWS     PC4 /* South direction button */
#define SWW     PC5 /* West direction button */
#define OS_CD   PB6 /* SD card slot (Pin change interrupt 6) */
#define SWC     PE7 /* central button */

#define REPEAT_START    60      /* after 600ms */
#define REPEAT_NEXT     10      /* every 100ms */
void init_encoder(void);

void scan_encoder(void);
void scan_switches(void);

int8_t os_enc_delta(void);

uint8_t get_switch_press( uint8_t switch_mask );
uint8_t get_switch_rpt( uint8_t switch_mask );
uint8_t get_switch_state( uint8_t switch_mask );
uint8_t get_switch_short( uint8_t switch_mask );
uint8_t get_switch_long( uint8_t switch_mask );
void clear_switches(void);
#endif /* ENCODER_H */