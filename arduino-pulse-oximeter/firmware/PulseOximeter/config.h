#pragma once
// ---------------------------------------------------------------
// Project configuration - edit here, not in the sketch.
// ---------------------------------------------------------------

// OLED panel: define exactly ONE.
//  - 128x32 : the UI is laid out for this size (default)
//  - 128x64 : 0.96" panels; the UI uses the top half only
#define SCREEN_128X32
// #define SCREEN_128X64

#define SSD1306_I2C_ADDR     0x3C   // some modules use 0x3D

#define BUTTON_PIN           3      // INT1 on ATmega328P (Nano/Uno)
#define EEPROM_ADDR_OPTIONS  7      // 2 bytes: filter_for_graph, draw_Red

#define FINGER_THRESHOLD     5000   // IR level below which "no finger"
#define BPM_MAX_VALID        200    // ignore beats faster than this
#define DISPLAY_REFRESH_MS   50
#define DISPLAY5_DURATION_MS 1100   // "average" screen after button press
#define DEBOUNCE_MS          70
