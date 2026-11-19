/*
* Filter output from MAX30102 and detect heartbeat
* Original: j.n.magee 15-10-2019
*/
#include "Pulse.h"

Pulse::Pulse() {
  cycle_max = 20;
  cycle_min = -20;
  positive = false;
  prev_sig = 0;
  amplitude_avg_total = 0;
}

// Returns true if a beat (signal peak) is detected
bool Pulse::isBeat(int16_t signal) {
  bool beat = false;
  // rising slope: track maximum
  if (positive && (signal > prev_sig)) cycle_max = signal;
  // falling slope: track minimum
  if (!positive && (signal < prev_sig)) cycle_min = signal;
  // rising -> falling = peak => beat
  if (positive && (signal < prev_sig)) {
    int amplitude = cycle_max - cycle_min;
    if (amplitude > 20 && amplitude < 3000) {
      beat = true;
      amplitude_avg_total += (amplitude - amplitude_avg_total / 4);
    }
    cycle_min = 0;
    positive = false;
  }
  // falling -> rising = valley
  if (!positive && (signal > prev_sig)) {
    cycle_max = 0;
    positive = true;
  }
  prev_sig = signal;
  return beat;
}
