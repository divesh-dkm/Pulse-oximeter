/*
* Filter output from MAX30102 and detect heartbeat
* Original: j.n.magee 15-10-2019
*/
#pragma once
#include <Arduino.h>

#define NSAMPLE 24  // DC-removal EMA filter, alpha = 1/NSAMPLE
#define NSLOT   4   // moving-average filter length

class MAFilter {
 public:
  MAFilter() : nextslot(0) { for (uint8_t i = 0; i < NSLOT; ++i) buffer[i] = 0; }

  int16_t filter(int16_t value) {
    buffer[nextslot] = value;
    nextslot = (nextslot + 1) % NSLOT;
    int16_t total = 0;
    for (int i = 0; i < NSLOT; ++i) total += buffer[i];
    return total / NSLOT;
  }

 private:
  int16_t buffer[NSLOT];
  uint8_t nextslot;
};

class DCFilter {
 public:
  DCFilter(void) : sample_avg_total(0) {}

  // remove DC component from a sample
  int16_t filter(int32_t sample) {
    sample_avg_total += (sample - sample_avg_total / NSAMPLE);
    return (int16_t)(sample - sample_avg_total / NSAMPLE);
  }
  int32_t avgDC() { return sample_avg_total / NSAMPLE; }

 private:
  int32_t sample_avg_total;
};

class Pulse {
 public:
  Pulse(void);
  int16_t dc_filter(int32_t sample) { return dc.filter(sample); }  // remove DC
  int16_t ma_filter(int16_t sample) { return ma.filter(sample); }  // low-pass
  bool    isBeat(int16_t signal);                                  // peak detector
  int32_t avgDC() { return dc.avgDC(); }
  int16_t avgAC() { return amplitude_avg_total / 4; }

 private:
  DCFilter dc;
  MAFilter ma;
  int16_t amplitude_avg_total;
  int16_t cycle_max;
  int16_t cycle_min;
  bool    positive;
  int16_t prev_sig;
};
