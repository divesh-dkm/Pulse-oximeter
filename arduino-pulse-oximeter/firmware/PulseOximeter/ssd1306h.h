#pragma once
// SSD1306 OLED driver - low-memory, single 128-byte page buffer.
// NOTE: this header was reconstructed from ssd1306h.cpp (the original header
// was not supplied). Default arguments live HERE, not in the .cpp.
#include <Arduino.h>
#include "config.h"

#define COLUMNS 128
#ifdef SCREEN_128X64
  #define PAGES 8
#else
  #define PAGES 4
#endif

class SSD1306 {
 public:
  SSD1306();
  void init();
  void fill(uint8_t data);
  void off();
  void on();

  // Paged drawing: call firstPage(), then draw + nextPage() in a do/while.
  void firstPage();
  bool nextPage();

  void drawPixel(uint8_t x, uint8_t y);
  void drawHLine(uint8_t x, uint8_t y, uint8_t w);
  void drawVLine(uint8_t x, uint8_t y, uint8_t h);
  void drawChar(int x, int y, unsigned char c, const int BIG = 1);  // BIG: 1 or 2
  void drawStr(int x, int y, const char *s, const int BIG = 1);
  void drawStr(int x, int y, const __FlashStringHelper *s, const int BIG = 1);
  void drawXBMP(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bitmap);
  void drawNibbles(uint8_t y, uint8_t nibbles[COLUMNS / 2]);

 private:
  void command_start();
  void command_stop();
  void command(uint8_t cmd);
  void data_start();
  void data_stop();
  void data_byte(uint8_t data);
  void set_area(uint8_t col, uint8_t page, uint8_t col_range, uint8_t page_range);
  void writePage(uint8_t page);
  bool inPage(uint8_t y, uint8_t h);

  uint8_t currentPage;
  uint8_t pageBuf[COLUMNS];
};
