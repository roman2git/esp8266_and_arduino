#include <Wire.h>
// https://github.com/Makuna/Rtc
#include <RtcDS1307.h>
#include "PinChangeInterrupt.h"

/*
  atmega328  -  esp - ds1307
  3v
  gnd
  scl - d2  - scl
  sda - d0  - sda // led
  d2 -      - sqw
  d6  - tx
  d7  - rst
  d12 -  -        // button to inform flashing of esp
*/

// pins
// IN
#define SQW_PIN         2   // int 0
#define BOOT_MODE_PIN   12  // put esp in booot mode
// OUT
#define LED_PIN         9
#define CHECK_TWI_PIN  6
#define RESET_ESP_PIN   7   // reset esp

typedef struct
{
  uint32_t _salt;
  uint32_t pls;
  uint16_t ct1;
  uint16_t ct2;
  uint16_t ct3;
  uint16_t pad;
} data;

data sensor_data;

RtcDS1307 Rtc;

volatile bool bsqw_pulse;
volatile bool bmode_start;
volatile bool breset_esp01;
volatile bool bboot_done;
volatile bool btwi_busy;
volatile bool btwi_written;

void twi_busy_isr() {
  btwi_busy = digitalRead(CHECK_TWI_PIN);
}

void sqw_isr() {
  bsqw_pulse = true;
  digitalWrite(LED_PIN, HIGH);
}

void boot_mode_isr() {
  bmode_start = !bmode_start;
  breset_esp01 = true;
}

void _reset_esp01(bool upload) {
  bboot_done = false;
  Wire.end();
  digitalWrite(RESET_ESP_PIN, LOW);
  if (upload == true) {
    pinMode(CHECK_TWI_PIN, INPUT_PULLUP);
    pinMode(SDA, OUTPUT);
    pinMode(SCL, INPUT_PULLUP);

    digitalWrite(SDA, LOW);
  } else {
    pinMode(SCL, INPUT_PULLUP);
    pinMode(SDA, INPUT_PULLUP);
  }

  delay(3);
  digitalWrite(RESET_ESP_PIN, HIGH);
  delay(200);

  if (upload == false) {
    bboot_done = true;
    Wire.begin();
  }
}

void start_measure() {
  //delay(200);
  delay(100);

  sensor_data._salt++;
  sensor_data.pls++;
  sensor_data.ct1++;
  sensor_data.ct2++;
  sensor_data.ct3++;
  sensor_data.pad = sensor_data._salt + sensor_data.pls + sensor_data.ct1 + sensor_data.ct2 + sensor_data.ct3 ;

  digitalWrite(LED_PIN, LOW);
  btwi_written = false;
}

void write_nvram() {

  detachPinChangeInterrupt(digitalPinToPinChangeInterrupt(CHECK_TWI_PIN));
  pinMode(CHECK_TWI_PIN, OUTPUT);
  digitalWrite(CHECK_TWI_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);
  delayMicroseconds(5);

  int start_address = 0;
  const uint8_t* to_write_current = reinterpret_cast<const uint8_t*>(&sensor_data);
  Rtc.SetMemory(start_address, to_write_current, sizeof(sensor_data));

  delayMicroseconds(5);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(CHECK_TWI_PIN, LOW);
  pinMode(CHECK_TWI_PIN, INPUT);
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(CHECK_TWI_PIN), twi_busy_isr, FALLING);
}

void setup() {
  // BOOL
  bsqw_pulse = bmode_start = breset_esp01 = bboot_done = btwi_busy = btwi_written = false;

  // OUT
  pinMode(LED_PIN, OUTPUT);
  pinMode(RESET_ESP_PIN, OUTPUT);

  // IN
  pinMode(SQW_PIN, INPUT_PULLUP);
  pinMode(BOOT_MODE_PIN, INPUT_PULLUP);
  pinMode(CHECK_TWI_PIN, INPUT);

  // OUT
  digitalWrite(RESET_ESP_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);

  // RESET
  _reset_esp01(false);
  delay(200);
  digitalWrite(LED_PIN, LOW);

  // sensor_data
  sensor_data._salt = 0;
  sensor_data.pls   = 10;
  sensor_data.ct1   = 20;
  sensor_data.ct2   = 30;
  sensor_data.ct3   = 40;
  sensor_data.pad   = sensor_data._salt + sensor_data.pls + sensor_data.ct1 + sensor_data.ct2 + sensor_data.ct3 ;

  // RTC
  Rtc.Begin();
  Rtc.SetIsRunning(true);
  Rtc.SetSquareWavePin(DS1307SquareWaveOut_1Hz);

  // INTERRUPT
  attachInterrupt(digitalPinToInterrupt(SQW_PIN), sqw_isr, FALLING);
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(BOOT_MODE_PIN), boot_mode_isr, FALLING);
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(CHECK_TWI_PIN), twi_busy_isr, CHANGE);
}

void loop() {
  if (bsqw_pulse) {
    start_measure();
    if (!breset_esp01 && bboot_done) {
      if (!btwi_written) {
        if (!btwi_busy) {
          write_nvram();
          btwi_written = true;
        }
      }
    }
    bsqw_pulse = false;
  }

  if (breset_esp01) {
    _reset_esp01(bmode_start);
    breset_esp01 = false;
  }
}