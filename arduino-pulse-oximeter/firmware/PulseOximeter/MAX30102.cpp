/*
*  MAX30102 driver - combination of ideas from the Maxim & SparkFun drivers
*  Original: j.n.magee 15-10-2019
*/
#include <Wire.h>
#include "MAX30102.h"

static const uint8_t MAX_30102_ID = 0x15;

MAX30102::MAX30102() {
  sense.head = 0;
  sense.tail = 0;
}

boolean MAX30102::begin(uint8_t i2caddr) {
  _i2caddr = i2caddr;
  return readRegister8(REG_PART_ID) == MAX_30102_ID;
}

void MAX30102::setup() {
  writeRegister8(REG_MODE_CONFIG, 0x40);  // reset
  delay(500);
  writeRegister8(REG_FIFO_WR_PTR, 0x00);
  writeRegister8(REG_OVF_COUNTER, 0x00);
  writeRegister8(REG_FIFO_RD_PTR, 0x00);
  writeRegister8(REG_FIFO_CONFIG, 0x4F);  // sample avg = 4, rollover off, almost-full = 15
  writeRegister8(REG_MODE_CONFIG, 0x03);  // 0x02 Red only, 0x03 SpO2 (Red+IR), 0x07 multi-LED
  writeRegister8(REG_SPO2_CONFIG, 0x27);  // ADC 4096 nA, 100 Hz, pulse width 411 us
  writeRegister8(REG_LED1_PA, 0x17);      // IR  LED ~4.6 mA (0x17 * 0.2 mA)
  writeRegister8(REG_LED2_PA, 0x17);      // Red LED ~4.6 mA
  writeRegister8(REG_PILOT_PA, 0x1F);     // Pilot LED ~6.2 mA
}

uint8_t MAX30102::available(void) {
  int8_t numberOfSamples = sense.head - sense.tail;
  if (numberOfSamples < 0) numberOfSamples += STORAGE_SIZE;
  return numberOfSamples;
}

uint32_t MAX30102::getRed(void) { return sense.red[sense.tail]; }
uint32_t MAX30102::getIR(void)  { return sense.IR[sense.tail]; }

void MAX30102::nextSample(void) {
  if (available()) {
    sense.tail++;
    sense.tail %= STORAGE_SIZE;
  }
}

// Poll the sensor FIFO and push any new samples into the ring buffer.
uint16_t MAX30102::check(void) {
  byte readPointer  = readRegister8(REG_FIFO_RD_PTR);
  byte writePointer = readRegister8(REG_FIFO_WR_PTR);
  int numberOfSamples = 0;
  if (readPointer != writePointer) {
    numberOfSamples = writePointer - readPointer;
    if (numberOfSamples < 0) numberOfSamples += 32;  // FIFO depth
    int bytesLeftToRead = numberOfSamples * 6;       // 3 bytes Red + 3 bytes IR
    Wire.beginTransmission(_i2caddr);
    Wire.write(REG_FIFO_DATA);
    Wire.endTransmission();
    bytesLeftToRead = bytesLeftToRead <= 32 ? bytesLeftToRead : 32;  // AVR Wire buffer = 32
    Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)bytesLeftToRead);
    while (bytesLeftToRead > 0) {
      sense.head++;
      sense.head %= STORAGE_SIZE;
      sense.IR[sense.head]  = readFIFOSample();
      sense.red[sense.head] = readFIFOSample();
      bytesLeftToRead -= 6;
    }
  }
  return numberOfSamples;
}

// ---- Low-level I2C ----
uint8_t MAX30102::readRegister8(uint8_t reg) {
  Wire.beginTransmission(_i2caddr);
  Wire.write((uint8_t)reg);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)1);
  return Wire.read();
}

uint32_t MAX30102::readFIFOSample() {
  byte temp[4];
  uint32_t temp32;
  temp[3] = 0;
  temp[2] = Wire.read();
  temp[1] = Wire.read();
  temp[0] = Wire.read();
  memcpy(&temp32, temp, 4);
  return temp32 & 0x3FFFF;  // 18-bit ADC value
}

void MAX30102::writeRegister8(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(_i2caddr);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}
