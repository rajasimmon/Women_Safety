/*
 * MAX30102 diagnostic sketch
 * Wiring: SDA=GPIO21, SCL=GPIO22, 3.3V, GND
 */

#include <Arduino.h>
#include <Wire.h>
#include "MAX30102.h"

#ifndef I2C_SPEED_STANDARD
#define I2C_SPEED_STANDARD 100000
#endif

#define MAX30102_SDA_PIN 21
#define MAX30102_SCL_PIN 22
#define MAX30102_I2C_ADDR 0x57
#define FINGER_THRESHOLD 10000

MAX30102 particleSensor(MAX30102_I2C_ADDR, Wire);

const byte RATE_SIZE = 8;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute = 0;
int beatAvg = 0;

bool detectBeatFromIR(int32_t sample) {
  static int32_t average = 0;
  static bool wasAboveThreshold = false;
  static unsigned long lastBeatTime = 0;

  if (average == 0) average = sample;
  average = ((average * 31) + sample) / 32;

  int32_t acSignal = sample - average;
  bool aboveThreshold = acSignal > 800;
  bool beatDetected = aboveThreshold && !wasAboveThreshold && (millis() - lastBeatTime > 300);

  if (beatDetected) lastBeatTime = millis();
  wasAboveThreshold = aboveThreshold;

  return beatDetected;
}

bool i2cPresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void scanI2C() {
  Serial.println("Scanning I2C bus...");
  byte found = 0;
  for (uint8_t address = 1; address < 127; address++) {
    if (i2cPresent(address)) {
      Serial.printf("I2C device found at 0x%02X\n", address);
      found++;
    }
  }
  if (!found) Serial.println("No I2C devices found. Check VCC, GND, SDA, SCL.");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(MAX30102_SDA_PIN, MAX30102_SCL_PIN);
  Wire.setClock(100000);
  scanI2C();

  if (!i2cPresent(MAX30102_I2C_ADDR)) {
    Serial.println("MAX30102 not found at 0x57.");
    return;
  }

  if (!particleSensor.begin()) {
    Serial.println("MAX30102 found on I2C, but library init failed.");
    return;
  }

  particleSensor.setLedCurrent(MAX30102::LED_RED, 0x3F);
  particleSensor.setLedCurrent(MAX30102::LED_IR, 0x3F);
  particleSensor.clearFIFO();

  Serial.printf("MAX30102 ready. Part ID: 0x%02X\n", particleSensor.readPartId());
  Serial.println("Put finger fully on the sensor and keep still.");
}

void loop() {
  MAX30102Sample sample = particleSensor.readSample(20);
  if (!sample.valid) return;

  long irValue = sample.ir;
  long redValue = sample.red;

  if (irValue > FINGER_THRESHOLD && detectBeatFromIR(irValue)) {
    unsigned long now = millis();
    long delta = now - lastBeat;
    lastBeat = now;

    if (delta > 300 && delta < 2000) {
      beatsPerMinute = 60 / (delta / 1000.0);
      if (beatsPerMinute > 35 && beatsPerMinute < 180) {
        rates[rateSpot++] = (byte)beatsPerMinute;
        rateSpot %= RATE_SIZE;

        beatAvg = 0;
        byte valid = 0;
        for (byte i = 0; i < RATE_SIZE; i++) {
          if (rates[i] > 0) {
            beatAvg += rates[i];
            valid++;
          }
        }
        if (valid) beatAvg /= valid;
      }
    }
  }

  if (irValue <= FINGER_THRESHOLD) {
    beatAvg = 0;
    rateSpot = 0;
    lastBeat = 0;
    memset(rates, 0, sizeof(rates));
  }

  Serial.printf("IR=%ld RED=%ld Finger=%s BPM=%d\n",
                irValue, redValue, irValue > FINGER_THRESHOLD ? "YES" : "NO", beatAvg);
  delay(250);
}
