# Arduino Pulse Oximeter (MAX30102 + OLED)

An Arduino-based heart-rate and blood-oxygen monitoring prototype using the **MAX30102 optical sensor**, **Arduino Nano (ATmega328P)** and **SSD1306 I²C OLED display**.

The system uses red and infrared photoplethysmography (PPG) signals to detect heartbeats and estimate blood oxygen saturation (SpO₂). Processed measurements are displayed on the OLED in real time.

**Author:** Divesh

> ⚠️ **Disclaimer:** This is an educational electronics and signal-processing project, not a medical device. The displayed heart-rate and SpO₂ values are approximate and have not been clinically validated. Do not use them for medical diagnosis or treatment decisions.

## Features

- Real-time heart-rate estimation in Beats Per Minute (BPM).
- Approximate SpO₂ estimation using red and infrared optical signals.
- Digital signal processing using DC removal and moving-average filters.
- Heartbeat detection using waveform peak identification.
- OLED-based display for pulse rate and oxygen saturation.
- PPG signal waveform recording and visualisation.
- Push-button interface for changing signal-processing and display options.
- EEPROM storage for user preferences.
- Automatic low-power sleep functionality after prolonged finger removal.

## Hardware Requirements

| Component | Description |
|---|---|
| Arduino Nano | ATmega328P microcontroller |
| MAX30102 | Optical heart-rate and SpO₂ sensor |
| SSD1306 OLED | I²C display, typically 128 × 32 or 128 × 64 |
| Push Button | User input and display control |
| Jumper Wires | Hardware connections |
| Breadboard | Prototype assembly |

## Pin Configuration

| Component | Pin | Arduino Nano |
|---|---|---|
| MAX30102 | SDA | A4 |
| MAX30102 | SCL | A5 |
| MAX30102 | VCC | As supported by breakout board |
| MAX30102 | GND | GND |
| OLED | SDA | A4 |
| OLED | SCL | A5 |
| OLED | VCC | As supported by display module |
| OLED | GND | GND |
| Push Button | One terminal | D3 |
| Push Button | Other terminal | GND |

**I²C Addresses:**
- MAX30102: `0x57`
- SSD1306 OLED: commonly `0x3C`

The MAX30102 and OLED share the same I²C communication bus.

Check the voltage requirements of your breakout boards before connecting them.

See [docs/WIRING.md](docs/WIRING.md) for the connection details.

## Software Requirements

- Arduino IDE
- Arduino AVR Boards package
- AVR-compatible compiler

The project uses the following standard Arduino/AVR components:

- `Wire.h` – I²C communication
- `EEPROM.h` – Persistent storage of settings
- `avr/pgmspace.h` – Storage of lookup tables in program memory
- `avr/sleep.h` – Low-power sleep functionality

The OLED implementation requires the compatible `ssd1306h` library used by the project.

## Repository Structure

```text
arduino-pulse-oximeter/
│
├── firmware/
│   └── PulseOximeter/
│       ├── PulseOximeter.ino
│       ├── MAX30102.h
│       ├── MAX30102.cpp
│       ├── Pulse.h
│       ├── Pulse.cpp
│       └── ssd1306h.h / ssd1306h.cpp
│
├── docs/
│   ├── WIRING.md
│   └── CODE_REVIEW.md
│
├── README.md
├── CHANGELOG.md
└── NOTICE.md
```

*The exact repository contents depend on the version of the firmware being published. Include the OLED driver files if distributing a standalone build.*

## Working Principle

The project follows a digital signal-processing approach to estimate heart rate and blood oxygen saturation.

### 1. Optical Signal Acquisition

The MAX30102 integrates:
- Red LED
- Infrared LED
- Photodetector
- Low-noise analogue front-end circuitry

The sensor measures changes in reflected red and infrared light caused by variations in blood volume.

### 2. Signal Processing

The acquired signals contain both DC and AC components.

The firmware applies:

- **DC Removal:** Removes the slowly varying baseline using an exponential moving-average filter.
- **Moving Average Filter:** Reduces signal fluctuations and high-frequency noise.
- **Peak Detection:** Identifies heartbeat peaks from changes in the filtered PPG waveform.

### 3. Heart Rate Calculation

The time interval between two consecutive detected heartbeats is used to calculate BPM.

\[
BPM = \frac{60000}{\Delta t}
\]

Where:

- \(\Delta t\) = Time interval between consecutive beats in milliseconds.
- BPM = Heart rate in beats per minute.

A moving-average filter is subsequently applied to stabilise the displayed reading.

### 4. SpO₂ Estimation

The system uses the ratio of red and infrared optical signals.

The ratio-of-ratios is approximated as:

\[
R = \frac{AC_{Red}/DC_{Red}}{AC_{IR}/DC_{IR}}
\]

The calculated ratio is mapped to an approximate SpO₂ value using an empirical lookup table.

The implementation also uses a linear approximation for displaying a filtered oxygen-saturation estimate.

**Note:** The algorithm is intended for educational demonstration and is not clinically calibrated.

### 5. OLED Display

The processed measurements are displayed on the SSD1306 OLED.

The display interface provides:
- Heart rate (BPM)
- Estimated oxygen saturation (%)
- Pulse waveform visualisation
- Additional average-reading screen

## Build and Upload

1. Install Arduino IDE.
2. Install the Arduino AVR Boards package.
3. Open the main `.ino` firmware file.
4. Ensure the required MAX30102, Pulse and SSD1306 driver files are available.
5. Select **Arduino Nano**.
6. Select the appropriate ATmega328P processor configuration.
7. Connect the Arduino Nano through USB.
8. Compile and upload the firmware.

If uploading fails, try the **ATmega328P (Old Bootloader)** processor option.

## Operating Instructions

1. Power on the Arduino Nano.
2. Wait for the initial OLED welcome screen.
3. Place your finger gently over the MAX30102 sensor.
4. Keep your finger stationary while the sensor acquires data.
5. Allow the signal-processing algorithm to stabilise.
6. Observe the estimated BPM and SpO₂ values on the OLED.
7. Use the push button to access the available display and filtering options.

## Applications

- Embedded systems development
- Biomedical instrumentation learning
- Photoplethysmography (PPG) signal processing
- Digital filter implementation
- Sensor interfacing using I²C
- Arduino-based healthcare monitoring prototypes

## Future Improvements

- Improve heartbeat detection under motion and noisy conditions.
- Implement signal-quality assessment.
- Improve SpO₂ estimation through proper calibration and validation.
- Add Bluetooth connectivity for wireless monitoring.
- Develop data logging and historical measurement analysis.
- Integrate a mobile application for visualising sensor readings.

## Acknowledgements

This project builds upon publicly available MAX30102 sensor-driver and pulse-processing implementations. Relevant upstream contributions and licensing information should be acknowledged in [NOTICE.md](NOTICE.md).

## Author

**Divesh**

Electronics and Embedded Systems Enthusiast

---

If you find this project useful for learning about Arduino, embedded systems and biomedical signal processing, feel free to explore the source code.
