# Code review

## Architecture

| File | Responsibility |
|------|----------------|
| `PulseOximeter.ino` | Main loop, UI screens, SpO₂ calculation, sleep, button |
| `MAX30102.*` | Sensor register setup, FIFO burst read into 3-slot ring buffer |
| `Pulse.*` | `DCFilter` (EMA, α = 1/24), `MAFilter` (4 taps), `Pulse::isBeat` peak detector |
| `ssd1306h.*` | Paged OLED renderer: draw calls only write to the *current* 8-pixel page; `do { … } while (oled.nextPage())` repeats drawing for each page so only 128 B RAM is needed |
| `font.h` | 5×8 font in flash; 2× scaling via bit-stretching |

Signal path: IR/Red (18-bit) → DC filter → (optional MA) → peak detector →
beat → interval → BPM; at each beat `R = (ACred/DCred)/(ACir/DCir)` →
SpO₂ ≈ 104 − 17·R (and an empirical table).

## Bugs found and fixed

| # | Severity | Issue | Status |
|---|----------|-------|--------|
| 1 | High | Wake from `PWR_DOWN` impossible: button interrupt is `CHANGE`; only LOW-level INTx can wake | Fixed |
| 2 | High | `ssd1306h.h` missing; `.cpp` redefines default args and uses `inline` across translation units → won't compile/link | Fixed (header reconstructed) |
| 3 | Medium | Display flicker: `draw_oled(5)` and `draw_oled(2)` both painted during the average screen | Fixed |
| 4 | Medium | `60000 / (now - lastBeat)` can divide by zero | Fixed |
| 5 | Medium | `#include <arduino.h>` fails on Linux/macOS case-sensitive builds | Fixed |
| 6 | Low | Stray `Wire.endTransmission()` after reads sends an extra I²C frame (slows loop) | Fixed |
| 7 | Low | Headers lacked include guards | Fixed |
| 8 | Low | `spo2_table[184]` holds 183 values; index 183 read 0 → SpO₂ 0 % | Fixed |
| 9 | Low | `istate` not restored from EEPROM → first button press could skip an option | Fixed |
| 10 | Low | Wrong comments (LED current, FIFO almost-full) | Fixed |
| 11 | Low | `drawPixel`/`drawChar` could write outside buffers | Fixed |

## Known limitations (not changed)

1. **Waveform is computed but never drawn.** `wave.record/scale` run every
   frame, yet no screen calls `wave.draw()`. As a result the `draw_Red` option
   only changes which channel drives beat detection. Either draw it (e.g. on a
   128×64 screen) or remove the Waveform code to save CPU/RAM.
2. **Effective sample rate is 25 Hz** (100 Hz ÷ 4 averaging). The peak
   detector's thresholds (`amplitude 20–3000`) are tuned for this; changing
   sensor settings requires retuning.
3. **Ring buffer is only 3 samples** and the loop blocks on OLED transfers
   (~4 page writes per frame at 400 kHz). Samples can be dropped when the
   display refreshes; beat timing therefore has jitter of tens of ms.
4. **SpO₂ is uncalibrated.** Both a linear formula (`SPO2f`, shown on the main
   screen) and a lookup table (`SPO2`, shown on the average screen) are used;
   they can disagree. Neither is clinically validated.
5. **`voltage` is measured but unused** – a natural place for a low-battery icon.
6. "OFF IN n s": the digit changes every 2 s, not every second.
7. `pinMode(0/2, INPUT)` before sleep is inherited from another board; on a
   Nano the I²C pins (A4/A5) matter more for sleep current.
8. Button option changes only work with *no finger* present.
9. Only `BIG` = 1 or 2 is supported for text scaling.
10. **The refactored code has not been compiled on hardware here** – build and
    test on your board before tagging a release.
