/*
 * Arduino Pulse Oximeter - MAX30102 + SSD1306 OLED
 *
 * Copyright (c) [YOUR NAME]
 * Based on work by j.n.magee (2019) and the DKARDU "heartrateNbloodoxygen"
 * project. See NOTICE.md.
 *
 * NOT A MEDICAL DEVICE - for education/hobby use only.
 */
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <EEPROM.h>

#include "config.h"
#include "ssd1306h.h"
#include "MAX30102.h"
#include "Pulse.h"

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#define LED LED_BUILTIN

// ---- Objects ----
SSD1306  oled;
MAX30102 sensor;
Pulse    pulseIR;
Pulse    pulseRed;
MAFilter bpm;

// ---- Globals ----
volatile uint8_t  pcflag = 0;       // set by button ISR
volatile uint32_t last_isr_ms = 0;  // debounce timer

int  beatAvg;
int  SPO2, SPO2f;
int  voltage;                       // VCC in 10 mV units (measured, not yet displayed)
bool filter_for_graph = false;
bool draw_Red = false;
uint8_t istate = 0;
uint8_t sleep_counter = 0;          // one tick per 200 ms while no finger
bool led_on = false;

unsigned long lastBeat = 0;
unsigned long displaytime = 0;
unsigned long display5_timer = 0;
bool display5_active = false;

// ---- SpO2 lookup table (index = R*100) ----
const uint8_t spo2_table[] PROGMEM = {
  95,95,95,96,96,96,97,97,97,97,97,98,98,98,98,98,99,99,99,99,
  99,99,99,99,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,99,99,99,99,99,99,99,99,98,98,98,98,98,98,97,97,
  97,97,96,96,96,96,95,95,95,94,94,94,93,93,93,92,92,92,91,91,
  90,90,89,89,89,88,88,87,87,86,86,85,85,84,84,83,82,82,81,81,
  80,80,79,78,78,77,76,76,75,74,74,73,72,72,71,70,69,69,68,67,
  66,66,65,64,63,62,62,61,60,59,58,57,56,56,55,54,53,52,51,50,
  49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,31,30,29,
  28,27,26,25,23,22,21,20,19,17,16,15,14,12,11,10,9,7,6,5,
  3,2,1
};

void draw_oled(int msg);

// ---- Measure VCC (10 mV units) using the 1.1 V bandgap ----
int getVCC() {
#if defined(__AVR_ATmega1284P__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#else
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);  // ATmega328
#endif
  delay(2);
  ADCSRA |= _BV(ADSC);                 // first conversion is discarded
  while (bit_is_set(ADCSRA, ADSC));
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC));
  uint8_t low = ADCL;                  // read ADCL first, then ADCH
  unsigned int val = (ADCH << 8) | low;
  if (val == 0) return 0;
  return (((long)1024 * 1100) / val) / 100;
}

// ---- Print number on OLED (leading zeros shown as blanks) ----
void print_digit(int x, int y, long val, char c = ' ', uint8_t field = 3, const int BIG = 2) {
  uint8_t ff = field;
  do {
    char ch = (val != 0) ? val % 10 + '0' : c;
    oled.drawChar(x + BIG * (ff - 1) * 6, y, ch, BIG);
    val /= 10;
    --ff;
  } while (ff > 0);
}

// ---- Waveform (recorded and scaled; draw() is available but not used by the UI) ----
const uint8_t MAXWAVE = 72;
class Waveform {
  public:
    Waveform() : wavep(0) {}

    void record(int waveval) {
      waveval /= 8;
      waveval += 128;
      waveval = waveval < 0 ? 0 : waveval;
      waveform[wavep] = (uint8_t)((waveval > 255) ? 255 : waveval);
      wavep = (wavep + 1) % MAXWAVE;
    }

    void scale() {
      uint8_t maxw = 0, minw = 255;
      for (int i = 0; i < MAXWAVE; i++) {
        if (waveform[i] > maxw) maxw = waveform[i];
        if (waveform[i] < minw) minw = waveform[i];
      }
      uint8_t scale8 = (maxw - minw) / 4 + 1;
      uint8_t index = wavep;
      for (int i = 0; i < MAXWAVE; i++) {
        disp_wave[i] = 31 - ((uint16_t)(waveform[index] - minw) * 8) / scale8;
        index = (index + 1) % MAXWAVE;
      }
    }

    void draw(uint8_t X) {
      for (int i = 0; i < MAXWAVE; i++) {
        uint8_t y = disp_wave[i];
        oled.drawPixel(X + i, y);
        if (i < MAXWAVE - 1) {
          uint8_t nexty = disp_wave[i + 1];
          if (nexty > y) {
            for (uint8_t iy = y + 1; iy < nexty; ++iy) oled.drawPixel(X + i, iy);
          } else if (nexty < y) {
            for (uint8_t iy = nexty + 1; iy < y; ++iy) oled.drawPixel(X + i, iy);
          }
        }
      }
    }

  private:
    uint8_t waveform[MAXWAVE];
    uint8_t disp_wave[MAXWAVE];
    uint8_t wavep;
} wave;

// ---- Button ISRs ----
void button(void) {                      // normal operation (CHANGE) with debounce
  uint32_t now = millis();
  if (now - last_isr_ms > DEBOUNCE_MS) {
    pcflag = 1;
    last_isr_ms = now;
  }
}
void wakeISR(void) {}                    // wake from power-down (LOW level)

void attachButtonInterrupt() {
  pcflag = 0;
  last_isr_ms = millis();
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button, CHANGE);
}

// ---- Handle button while no finger is present: cycle display/filter option ----
void checkbutton() {
  if (pcflag && !digitalRead(BUTTON_PIN)) {
    istate = (istate + 1) % 4;
    filter_for_graph = istate & 0x01;
    draw_Red = istate & 0x02;
    EEPROM.update(EEPROM_ADDR_OPTIONS, filter_for_graph);  // update() avoids needless writes
    EEPROM.update(EEPROM_ADDR_OPTIONS + 1, draw_Red);
  }
  pcflag = 0;
}

// ---- Non-blocking "average" screen shown after a button press ----
void Display_5() {
  if (pcflag && !digitalRead(BUTTON_PIN)) {
    display5_active = true;
    display5_timer = millis();
    pcflag = 0;
  }
  if (display5_active) {
    draw_oled(5);
    if (millis() - display5_timer > DISPLAY5_DURATION_MS) display5_active = false;
  }
}

// ---- OLED screens ----
void draw_oled(int msg) {
  oled.firstPage();
  do {
    switch (msg) {
      case 0:  oled.drawStr(10, 0, F("Device error"), 1); break;
      case 1:  oled.drawStr(0, 0, F("PLACE YOUR"), 2);
               oled.drawStr(25, 18, F("FINGER"), 2); break;
      case 2:  print_digit(86, 0, beatAvg);
               oled.drawStr(0, 3, F("PULSE RATE"), 1);
               oled.drawStr(11, 17, F("OXYGEN"), 1);
               oled.drawStr(0, 25, F("SATURATION"), 1);
               print_digit(73, 16, SPO2f, ' ', 3, 2);
               oled.drawChar(116, 16, '%', 2); break;
      case 3:  oled.drawStr(33, 0, F("Pulse"), 2);
               oled.drawStr(17, 15, F("Oximeter"), 2); break;
      case 4:  oled.drawStr(28, 12, F("OFF IN"), 1);
               oled.drawChar(76, 12, 10 - sleep_counter / 10 + '0');
               oled.drawChar(82, 12, 's'); break;
      case 5:  oled.drawStr(0, 0, F("Avg Pulse"), 1);
               print_digit(75, 0, beatAvg);
               oled.drawStr(0, 15, F("AVG OXYGEN"), 1);
               oled.drawStr(0, 22, F("saturation"), 1);
               print_digit(75, 15, SPO2); break;
    }
  } while (oled.nextPage());
}

// ---- Hardware (re)initialisation: used at boot and after waking ----
void initHardware() {
  sbi(ADCSRA, ADEN);
  pinMode(LED, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  filter_for_graph = (EEPROM.read(EEPROM_ADDR_OPTIONS) == 1);
  draw_Red         = (EEPROM.read(EEPROM_ADDR_OPTIONS + 1) == 1);
  istate = (draw_Red ? 2 : 0) | (filter_for_graph ? 1 : 0);   // keep in sync with saved options

  oled.init();                          // also starts Wire - must precede sensor.begin()
  oled.fill(0x00);
  draw_oled(3);
  delay(3000);
  if (!sensor.begin()) {
    draw_oled(0);
    while (1);
  }
  sensor.setup();
}

// ---- Power-down sleep; wakes on button press (LOW level on INT1) ----
void go_sleep() {
  oled.fill(0);
  oled.off();
  delay(10);
  sensor.off();
  delay(10);
  cbi(ADCSRA, ADEN);
  delay(10);
  pinMode(0, INPUT);   // carried over from the original; harmless on a Nano
  pinMode(2, INPUT);

  // In SLEEP_MODE_PWR_DOWN only a LOW-level external interrupt can wake the
  // AVR (CHANGE/edge interrupts cannot), so switch trigger mode before sleeping.
  detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), wakeISR, LOW);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_mode();                         // ---- sleeps here ----
  detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));

  initHardware();
  attachButtonInterrupt();
}

// ---- Setup ----
void setup(void) {
  initHardware();
  attachButtonInterrupt();
}

// ---- Loop ----
void loop() {
  sensor.check();
  unsigned long now = millis();
  if (!sensor.available()) return;

  uint32_t irValue  = sensor.getIR();
  uint32_t redValue = sensor.getRed();
  sensor.nextSample();

  if (irValue < FINGER_THRESHOLD) {
    // ---- No finger ----
    voltage = getVCC();
    checkbutton();
    draw_oled(sleep_counter <= 50 ? 1 : 4);
    delay(200);
    ++sleep_counter;
    if (sleep_counter > 100) {          // ~20 s idle
      go_sleep();
      sleep_counter = 0;
    }
  } else {
    // ---- Finger present ----
    sleep_counter = 0;
    int16_t IR_signal, Red_signal;
    bool beatRed, beatIR;

    if (!filter_for_graph) {
      IR_signal  = pulseIR.dc_filter(irValue);
      Red_signal = pulseRed.dc_filter(redValue);
      beatRed = pulseRed.isBeat(pulseRed.ma_filter(Red_signal));
      beatIR  = pulseIR.isBeat(pulseIR.ma_filter(IR_signal));
    } else {
      IR_signal  = pulseIR.ma_filter(pulseIR.dc_filter(irValue));
      Red_signal = pulseRed.ma_filter(pulseRed.dc_filter(redValue));
      beatRed = pulseRed.isBeat(Red_signal);
      beatIR  = pulseIR.isBeat(IR_signal);
    }

    wave.record(draw_Red ? -Red_signal : -IR_signal);

    if (draw_Red ? beatRed : beatIR) {
      unsigned long dt = now - lastBeat;
      if (dt > 0) {                                    // guard: avoid divide-by-zero
        long btpm = 60000UL / dt;
        if (btpm > 0 && btpm < BPM_MAX_VALID) beatAvg = bpm.filter((int16_t)btpm);
      }
      lastBeat = now;
      digitalWrite(LED, HIGH);
      led_on = true;

      // Ratio-of-ratios: R = (AC_red/DC_red) / (AC_ir/DC_ir)
      long numerator   = ((long)pulseRed.avgAC() * pulseIR.avgDC()) / 256;
      long denominator = ((long)pulseRed.avgDC() * pulseIR.avgAC()) / 256;
      int RX100 = (denominator > 0) ? (numerator * 100) / denominator : 999;

      SPO2f = (10400 - RX100 * 17 + 50) / 100;        // linear approximation: 104 - 17R
      SPO2f = constrain(SPO2f, 0, 100);

      if (RX100 >= 0 && RX100 < (int)sizeof(spo2_table))
        SPO2 = pgm_read_byte_near(&spo2_table[RX100]); // empirical table
    }

    // Don't repaint the main screen while the "average" screen is showing (avoids flicker)
    if (!display5_active && (now - displaytime > DISPLAY_REFRESH_MS)) {
      displaytime = now;
      wave.scale();
      draw_oled(2);
    }
    Display_5();
  }

  if (led_on && (now - lastBeat) > 25) {
    digitalWrite(LED, LOW);
    led_on = false;
  }
}
