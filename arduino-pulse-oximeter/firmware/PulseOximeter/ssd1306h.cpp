/*
*  SSD1306 driver - optimised for low memory
*                   uses 1 128 byte page buffer
*
* Original: j.n.magee 15-10-2019
*
* Uses initialisation ideas from:
* DigisparkOLED: https://github.com/digistump/DigistumpArduino/tree/master/digistump-avr/libraries/DigisparkOLED
* SSD1306 data sheet: https://www.adafruit.com/datasheets/SSD1306.pdf
*/
#include <avr/pgmspace.h>
#include <Wire.h>
#include "ssd1306h.h"
#include "font.h"

// Software configuration, data sheet page 64
static const uint8_t ssd1306_configuration[] PROGMEM = {
#ifdef SCREEN_128X64
  0xA8, 0x3F,   // Set MUX Ratio
#else
  0xA8, 0x1F,   // Set MUX Ratio
#endif
  0xD3, 0x00,   // Set Display Offset
  0x40,         // Set Display Start line
  0xA1,         // Segment re-map (mirror)
  0xC8,         // COM output scan direction (flip)
#ifdef SCREEN_128X64
  0xDA, 0x12,   // COM pins: alternative
#else
  0xDA, 0x02,   // COM pins: sequential
#endif
  0x81, 0xFF,   // Contrast
  0xA4,         // Output follows RAM
  0xA6,         // Normal (not inverted)
  0xD5, 0x80,   // Oscillator frequency
  0x8D, 0x14,   // Enable charge pump
  0xAF          // Display ON
};

SSD1306::SSD1306() : currentPage(0) {}

void SSD1306::init() {
  Wire.begin();
  Wire.setClock(400000);
  for (uint8_t i = 0; i < sizeof(ssd1306_configuration); i++) {
    command(pgm_read_byte_near(&ssd1306_configuration[i]));
  }
  currentPage = 0;
  memset(pageBuf, 0x00, COLUMNS);
}

void SSD1306::command_start() {
  Wire.beginTransmission(SSD1306_I2C_ADDR);
  Wire.write(0x00);  // command
}
void SSD1306::command_stop() { Wire.endTransmission(); }

void SSD1306::command(uint8_t cmd) {
  command_start();
  Wire.write(cmd);
  command_stop();
}

void SSD1306::data_start() {
  Wire.beginTransmission(SSD1306_I2C_ADDR);
  Wire.write(0x40);  // data
}
void SSD1306::data_stop() { Wire.endTransmission(); }

void SSD1306::data_byte(uint8_t data) {
  if (Wire.write(data) == 0) {  // Wire buffer full: flush and restart
    data_stop();
    data_start();
    Wire.write(data);
  }
}

void SSD1306::set_area(uint8_t col, uint8_t page, uint8_t col_range, uint8_t page_range) {
  command_start();
  Wire.write(0x20); Wire.write(0x01);                       // vertical addressing
  Wire.write(0x21); Wire.write(col); Wire.write(col + col_range - 1);
  Wire.write(0x22); Wire.write(page); Wire.write(page + page_range - 1);
  command_stop();
}

void SSD1306::fill(uint8_t data) {
  set_area(0, 0, COLUMNS, PAGES);
  uint16_t size = (COLUMNS) * (PAGES);
  data_start();
  for (uint16_t i = 0; i < size; i++) data_byte(data);
  data_stop();
}

void SSD1306::writePage(uint8_t page) {
  set_area(0, page, COLUMNS, 1);
  data_start();
  for (uint8_t i = 0; i < COLUMNS; i++) data_byte(pageBuf[i]);
  data_stop();
}

bool SSD1306::inPage(uint8_t y, uint8_t h) {
  return currentPage >= y / 8 && currentPage <= (y + h - 1) / 8;
}

void SSD1306::off() { command(0xAE); }
void SSD1306::on()  { command(0xAF); }

void SSD1306::drawPixel(uint8_t x, uint8_t y) {
  if (x >= COLUMNS || y / 8 != currentPage) return;
  pageBuf[x] |= 1 << (y % 8);
}

void SSD1306::drawHLine(uint8_t x, uint8_t y, uint8_t w) {
  if (y / 8 != currentPage) return;
  for (uint8_t i = 0; i < w && (x + i) < COLUMNS; ++i) pageBuf[x + i] |= 1 << (y % 8);
}

void SSD1306::drawVLine(uint8_t x, uint8_t y, uint8_t h) {
  for (uint8_t i = 0; i < h; ++i) drawPixel(x, y + i);
}

// Double each bit of an 8-bit column -> 16-bit column (for 2x text)
static uint16_t Stretch(uint16_t x) {
  x = (x & 0xF0) << 4 | (x & 0x0F);
  x = (x << 2 | x) & 0x3333;
  x = (x << 1 | x) & 0x5555;
  return x | x << 1;
}

void SSD1306::drawChar(int x, int y, unsigned char c, const int BIG) {
  if (c < FONT_START || c > FONT_END) return;  // out-of-range guard
  uint16_t data;
  for (uint8_t i = 0; i < FONT_WIDTH; i++) {
    data = pgm_read_byte_near(&font_bitmap[c - FONT_START][i]);
    if (BIG == 2) data = Stretch(data);
    for (uint8_t d = 0; d < BIG; d++) {
      for (uint8_t j = 0; j < FONT_HEIGHT * BIG; j++) {
        if (data & (1 << j)) drawPixel(x + (i * BIG + d), y + j);
      }
    }
  }
}

void SSD1306::drawStr(int x, int y, const char *s, const int BIG) {
  if (!inPage(y, FONT_HEIGHT * BIG)) return;
  for (int k = 0; s[k] != '\0'; ++k) {
    drawChar(x, y, s[k], BIG);
    x += (FONT_WIDTH + 1) * BIG;
  }
}

void SSD1306::drawStr(int x, int y, const __FlashStringHelper *s, const int BIG) {
  if (!inPage(y, FONT_HEIGHT * BIG)) return;
  PGM_P p = reinterpret_cast<PGM_P>(s);
  while (1) {
    unsigned char c = pgm_read_byte(p++);
    if (c == 0) break;
    drawChar(x, y, c, BIG);
    x += (FONT_WIDTH + 1) * BIG;
  }
}

// Assumes each bitmap row is padded to whole bytes (LSB = leftmost pixel)
void SSD1306::drawXBMP(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bitmap) {
  uint8_t data = 0;
  if (!inPage(y, h)) return;
  uint8_t bytewidth = (w % 8 == 0) ? w / 8 : w / 8 + 1;
  for (int j = 0; j < h; ++j) {
    for (int i = 0; i < w; ++i) {
      uint8_t bitno = i % 8;
      if (bitno == 0) data = pgm_read_byte_near(&bitmap[j * bytewidth + i / 8]);
      if (data & (1 << bitno)) drawPixel(x + i, y + j);
    }
  }
}

void SSD1306::drawNibbles(uint8_t y, uint8_t nibbles[COLUMNS / 2]) {
  if (y / 8 != currentPage) return;
  uint8_t high = (y % 8 == 4) ? 1 : 0;
  for (int i = 0; i < COLUMNS / 2; i++) {
    if (high == 0) {
      pageBuf[i * 2]     |= nibbles[i] & 0x0F;
      pageBuf[i * 2 + 1] |= (nibbles[i] & 0xF0) >> 4;
    } else {
      pageBuf[i * 2]     |= (nibbles[i] & 0x0F) << 4;
      pageBuf[i * 2 + 1] |= nibbles[i] & 0xF0;
    }
  }
}

void SSD1306::firstPage() { currentPage = 0; }

bool SSD1306::nextPage() {
  writePage(currentPage++);
  memset(pageBuf, 0x00, COLUMNS);
  return currentPage != PAGES;
}
