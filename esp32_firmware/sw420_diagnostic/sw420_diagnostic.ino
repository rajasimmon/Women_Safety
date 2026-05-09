/*
 * SW-420 vibration diagnostic sketch
 * Wiring: DO=GPIO35, VCC=3.3V, GND=GND
 */

#include <Arduino.h>

#define VIBRATION_PIN 35
#define DEBOUNCE_MS 50
#define CALIBRATION_MS 1000

volatile bool ready = false;
volatile bool idleLevel = LOW;
volatile unsigned long lastInterruptMicros = 0;
volatile unsigned int pendingHits = 0;

void IRAM_ATTR onVibration() {
  if (!ready) return;

  unsigned long nowMicros = micros();
  if (nowMicros - lastInterruptMicros < DEBOUNCE_MS * 1000UL) return;

  bool raw = digitalRead(VIBRATION_PIN);
  if (raw != idleLevel) {
    pendingHits++;
    lastInterruptMicros = nowMicros;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(VIBRATION_PIN, INPUT);
  Serial.println("Keep SW-420 still for calibration...");

  unsigned long start = millis();
  unsigned int highSamples = 0;
  unsigned int lowSamples = 0;
  while (millis() - start < CALIBRATION_MS) {
    if (digitalRead(VIBRATION_PIN) == HIGH) highSamples++;
    else lowSamples++;
    delay(1);
  }

  idleLevel = highSamples >= lowSamples ? HIGH : LOW;
  lastInterruptMicros = micros();
  ready = true;
  attachInterrupt(digitalPinToInterrupt(VIBRATION_PIN), onVibration, CHANGE);

  Serial.printf("SW-420 ready. Idle level=%d. Tap the module now.\n", idleLevel);
}

void loop() {
  noInterrupts();
  unsigned int hits = pendingHits;
  pendingHits = 0;
  interrupts();

  if (hits > 0) {
    Serial.printf("Vibration detected. Hits=%u Raw=%d\n", hits, digitalRead(VIBRATION_PIN));
  }

  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 1000) {
    lastDebug = millis();
    Serial.printf("Raw=%d Idle=%d\n", digitalRead(VIBRATION_PIN), idleLevel);
  }
}
