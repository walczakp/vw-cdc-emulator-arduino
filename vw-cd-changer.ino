/*
 * VW Changer mini-ISO port -> my connection -> Arduino Nano:
 * 13 -> data in -> yellow -> D11 (MOSI)
 * 14 -> data out -> blue -> D12 (MISO)
 * 15 -> clock -> white -> D13 (SPI SCK)
 * 18 -> gnd -> gray -> GND
*/
#include <SPI.h>

#define DEBUG 1

#define MODE_PLAY 0xFF
#define MODE_SHUFFLE 0x55
#define MODE_SCAN 0x00

uint8_t transmit_msg(const uint8_t *in)
{
  // 62,5 kHz, MSB First, Cycle Start, CPOL=0, CPHA=1
  SPI.beginTransaction(SPISettings(62500, MSBFIRST, SPI_MODE1));
  for (int i=0; i<8; i++) {
    SPI.transfer(in[i]);
    delay(2);
  }
  SPI.endTransaction();
}

void print_hex(uint8_t *data, uint8_t sz) {
  #ifdef DEBUG
  for(int i=0; i<sz; i++) {
    Serial.print("0x");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  #endif //DEBUG
}

void setup() {
  #ifdef DEBUG
  Serial.begin(9600);
  Serial.println("Started!");
  #endif //DEBUG
  
  SPI.begin();
}

#ifdef DEBUG
uint8_t debug_loop_counter = 0;
uint8_t trackno = 1;
uint8_t cdno = 1;
#endif //DEBUG


void loop() {
  uint8_t cd = 0xFF ^ cdno; // max 16 cds
  uint8_t track = 0xFF ^ trackno; // max 255 tracks
  uint8_t msg[] = {0x34, cd, track, 0xFF /*minutes*/, 0xFF /*seconds*/, MODE_PLAY, 0xCF, 0x3C};
  transmit_msg(msg);
  delay(40);
 
  #ifdef DEBUG 
  if (debug_loop_counter % (1000/40) == 0) {
    trackno = (trackno<25) ? trackno+1 : 0;
    cdno = (cdno<15) ? cdno+1 : 0;
  }
  debug_loop_counter++;
  #endif //DEBUG
}
