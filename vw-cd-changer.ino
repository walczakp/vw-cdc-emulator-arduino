/*
 * VW Changer mini-ISO port -> my connection -> Arduino Nano 16MHz/5V:
 * 13 -> data in -> yellow -> D11 (MOSI)
 * 14 -> data out -> blue -> D2 - for interrupt
 * 15 -> clock -> white -> D13 (SPI SCK)
 * 18 -> gnd -> gray -> GND
 * 
 * based on: https://martinsuniverse.de/projekte/cdc_protokoll/cdc_protokoll.html
*/
#include <SPI.h>

#define DEBUG 1
#define RADIO_IN_PIN 2

#define MODE_PLAY 0xFF
#define MODE_SHUFFLE 0x55
#define MODE_SCAN 0x00

#define START_BIT_HIGH_US 8000
#define START_BIT_LOW_US 4000
#define BIT_0_LOW_US 500
#define BIT_0_HIGH_US 500
#define BIT_1_LOW_US 1500
#define BIT_1_HIGH_US 500

#define TARGET_COMMAND_REPEATS 5
#define HAS_COMMAND bits_read == 32 && command
#define RESET_COMMAND command = 0; bits_read = 0

uint8_t track = 1;
uint8_t cd = 1;

volatile uint8_t input_pin_state = LOW;
volatile uint8_t msg_started = 0;
volatile uint16_t high_state_time = 0;
volatile uint16_t low_state_time = 0;
volatile uint32_t command = 0;
volatile uint8_t bits_read = 0;

void INT_on_receive() {
  if (input_pin_state == HIGH) {
    // changed high -> low
    high_state_time = TCNT1 / 2;
    TCNT1 = 0;
   
    input_pin_state = LOW;
  } else {
    // changed low -> high   
    if (msg_started)
      low_state_time = TCNT1 / 2;
    else
      low_state_time = 0; // reset low time - don't count no message periods
      msg_started = 1;
    TCNT1 = 0;

    if (msg_started) {
      if (high_state_time > START_BIT_HIGH_US && low_state_time > START_BIT_LOW_US) {
        RESET_COMMAND;
      } else if (high_state_time > BIT_1_HIGH_US && low_state_time > BIT_1_LOW_US) {
        command = (command << 1) | 0x00000001;    // append bit 1
        bits_read++;
      } else if (high_state_time > BIT_0_HIGH_US && low_state_time > BIT_0_LOW_US) {
        command = (command << 1);    // append bit 0 - just lshift
        bits_read++;
      }
      
      if (HAS_COMMAND) {
        msg_started = 0;
      }
    }
    input_pin_state = HIGH;
  }
}

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
  
  pinMode(RADIO_IN_PIN, INPUT);
  TCCR1A = 0b00000000;
  TCCR1B = 0b00000010;  // enable Timer1 - 16 MHz, prescaler 8, 1 tick/2us
  TCNT1  = 0;           // reset Timer1
  
  attachInterrupt(digitalPinToInterrupt(RADIO_IN_PIN), INT_on_receive, CHANGE);
  
  SPI.begin();
}

uint32_t prev_command = 0;
uint8_t command_repeats = 0;
void loop() {
  uint8_t cd_inverted = 0xFF ^ cd; // max 16 cds
  uint8_t track_inverted = 0xFF ^ track; // max 255 tracks
  uint8_t msg[] = {0x34, cd_inverted, track_inverted, 0xFF /*minutes?*/, 0xFF /*seconds*/, MODE_PLAY, 0xCF, 0x3C};
  transmit_msg(msg);
  delay(40);

  if (HAS_COMMAND) {
//    noInterrupts();
//    if (command == prev_command) {
//      command_repeats++;
//    }
//    if (command_repeats == TARGET_COMMAND_REPEATS || prev_command != command) {
      Serial.print("CMD: ");
      Serial.println(command, HEX);
  //    track = (track<99) ? track+1 : 0;
//      command_repeats = 0;
//      prev_command = 0;
//    }
//    prev_command = command;
    RESET_COMMAND;
//    interrupts();
  }
  
}
