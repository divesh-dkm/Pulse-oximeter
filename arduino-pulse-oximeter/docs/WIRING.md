# Wiring (Arduino Nano)

Both I²C devices share the same two wires.

| Signal | Nano | MAX30102 | OLED | Button |
|--------|------|----------|------|--------|
| 3.3 V / 5 V | 3V3 or 5V* | VIN | VCC | – |
| GND | GND | GND | GND | one leg |
| SDA | A4 | SDA | SDA | – |
| SCL | A5 | SCL | SCL | – |
| Button | D3 | – | – | other leg |

\* Check your breakout boards: the MAX30102 core runs at 1.8 V/3.3 V; most
modules include regulators and level shifting for 5 V, but some do not. When in
doubt, power both from 3.3 V.

- The MAX30102 `INT` pin is not used (the FIFO is polled).
- D3 is INT1 on the ATmega328P – required for wake-from-sleep. Don't move it
  to another pin without re-doing the interrupt setup.
- I²C addresses: MAX30102 `0x57`, SSD1306 `0x3C`.
