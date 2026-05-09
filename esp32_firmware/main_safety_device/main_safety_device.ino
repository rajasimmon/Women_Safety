/*
 * ============================================================
 * IoT SMART WEARABLE SAFETY DEVICE FOR WOMEN
 * ============================================================
 * Hardware: ESP32 + MAX30102 (Heartbeat/SpO2) + DHT11 Temperature + SOS Button
 *           + SW-420 Vibration + AI Thinker VC-02 Voice Module
 *           + NEO-6M GPS + SIM800L GSM + Buzzer + LED
 * 
 * Author  : Smart Safety Systems
 * Version : 3.0
 * ============================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <DHT.h>
#include <Preferences.h>

#ifndef I2C_SPEED_STANDARD
#define I2C_SPEED_STANDARD 100000
#endif

#ifndef I2C_SPEED_FAST
#define I2C_SPEED_FAST 400000
#endif

// ============================================================
// PIN DEFINITIONS
// ============================================================
#define BUZZER_PIN          26
#define LED_RED_PIN         27
#define LED_GREEN_PIN       14
#define LED_BLUE_PIN        13
#define SOS_BUTTON_PIN      25
#define VIBRATION_PIN       35
#define VC02_RX_PIN         17  // ESP32 RX, connect to VC-02 TX
#define VC02_TX_PIN         16  // ESP32 TX, connect to VC-02 RX
#define GSM_RX_PIN          32
#define GSM_TX_PIN          23
#define GPS_RX_PIN          18
#define GPS_TX_PIN          19
#define BATTERY_ADC_PIN     34
#define MAX30102_SDA_PIN    21
#define MAX30102_SCL_PIN    22
#define MAX30102_I2C_ADDR   0x57
#define MAX30102_SAMPLE_AVERAGE  4
#define MAX30102_SAMPLE_RATE     100
#define MAX30102_LED_MODE        2
#define MAX30102_PULSE_WIDTH     411
#define MAX30102_ADC_RANGE       4096
#define DHT_PIN             33
#define DHT_TYPE            DHT11

// ============================================================
// CONFIGURATION
// ============================================================
const char* WIFI_SSID       = "simmon";
const char* WIFI_PASSWORD   = "raja1234";
const String SERVER_URL     = "http://10.52.161.81:8080/womens_safety/api/";
const String DEVICE_ID      = "WIFE_ESP32_001";

// Emergency contacts (up to 3)
String emergencyContacts[3] = {
  "+918883623623",   // Primary contact
  "+91XXXXXXXXXX",   // Secondary contact
  "+91XXXXXXXXXX"    // Tertiary contact (Police/Guardian)
};

// ============================================================
// THRESHOLDS & TIMING
// ============================================================
#define HEART_RATE_HIGH_THRESHOLD    130
#define HEART_RATE_LOW_THRESHOLD     45
#define MAX30102_FINGER_THRESHOLD    50000
#define MAX30102_LED_POWER           0x0A
#define VIBRATION_COUNT_THRESHOLD    3
#define VIBRATION_WINDOW_MS          3000
#define VIBRATION_DEBOUNCE_MS        50
#define VIBRATION_IDLE_CALIBRATION_MS 1000
#define ALERT_COOLDOWN_MS            30000  // 30 seconds
#define GPS_UPDATE_INTERVAL          10000  // 10 seconds
#define HEARTBEAT_UPDATE_INTERVAL    2000   // 2 seconds
#define SENSOR_READ_INTERVAL         1000   // 1 second
#define BODY_TEMP_UPDATE_INTERVAL    2000   // DHT11 needs slow reads
#define WIFI_RECONNECT_INTERVAL      15000  // 15 seconds
#define DATA_SEND_INTERVAL           2000   // 2 seconds

// ============================================================
// VC-02 VOICE RECOGNITION CONFIGURATION
// ============================================================
#define VC02_BAUD_RATE        9600
#define VC02_FRAME_HEADER     0x5A
#define VC02_FRAME_LENGTH     5
#define VC02_FRAME_TIMEOUT_MS 50
#define VC02_HELP_ME_COMMAND       0x01  // "help me"
#define VC02_EMERGENCY_COMMAND     0x02  // "emergency"
#define VC02_SAVE_ME_COMMAND       0x03  // "save me"
#define VC02_DANGER_COMMAND        0x04  // "danger"
#define VC02_CALL_POLICE_COMMAND   0x05  // "call police"
#define VC02_RAW_DEBUG        true
#define VC02_ZERO_WARN_MS     3000
#define VC02_REPEAT_WARN_MS   3000
#define VC02_REPEAT_WARN_COUNT 8

// ============================================================
// ALERT TYPE DEFINITIONS
// ============================================================
#define ALERT_NONE            0
#define ALERT_SOS_BUTTON      1
#define ALERT_VOICE_COMMAND   2
#define ALERT_HEART_RATE_HIGH 3
#define ALERT_HEART_RATE_LOW  4
#define ALERT_VIBRATION       5
#define ALERT_GEOFENCE        6

// ============================================================
// OBJECTS
// ============================================================
MAX30105 particleSensor;
DHT dht(DHT_PIN, DHT_TYPE);
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
HardwareSerial gsmSerial(2);
SoftwareSerial vc02Serial(VC02_RX_PIN, VC02_TX_PIN);
Preferences preferences;
bool max30102Ready = false;

// ============================================================
// STATE VARIABLES
// ============================================================
struct DeviceState {
  float latitude;
  float longitude;
  float altitude;
  float speed;
  float heartRate;
  float spo2;
  float temperature;
  uint8_t lastVoiceCommand;
  uint8_t lastVoiceByte;
  int vibrationCount;
  int batteryLevel;
  bool gpsFixed;
  bool wifiConnected;
  bool gsmConnected;
  bool voiceCommandDetected;
  bool emergencyActive;
  bool alertSent;
  int alertType;
  String lastAlertTime;
  unsigned long lastDataSend;
  unsigned long lastGPSUpdate;
  unsigned long lastHeartbeatUpdate;
  unsigned long lastAlertTime_ms;
  unsigned long lastVibrationTime;
  unsigned long systemUptime;
};

DeviceState state;

// Heart Rate tracking arrays
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;

// Vibration counting
int vibrationWindowCount = 0;
unsigned long vibrationWindowStart = 0;
volatile bool vibrationInterruptReady = false;
volatile bool vibrationIdleLevel = LOW;
volatile uint16_t vibrationPendingEdges = 0;
volatile unsigned long vibrationLastInterruptMicros = 0;

void IRAM_ATTR handleVibrationInterrupt() {
  if (!vibrationInterruptReady) return;

  unsigned long nowMicros = micros();
  if (nowMicros - vibrationLastInterruptMicros < (VIBRATION_DEBOUNCE_MS * 1000UL)) {
    return;
  }

  bool rawLevel = digitalRead(VIBRATION_PIN);
  if (rawLevel != vibrationIdleLevel) {
    vibrationPendingEdges++;
    vibrationLastInterruptMicros = nowMicros;
  }
}

void addHeartRateSample(long irValue, long redValue) {
  if (irValue > MAX30102_FINGER_THRESHOLD) {
    if (checkForBeat(irValue)) {
      long delta = millis() - lastBeat;
      lastBeat = millis();

      beatsPerMinute = 60 / (delta / 1000.0);

      if (beatsPerMinute < 255 && beatsPerMinute > 20) {
        rates[rateSpot++] = (byte)beatsPerMinute;
        rateSpot %= RATE_SIZE;

        beatAvg = 0;
        for (byte x = 0; x < RATE_SIZE; x++) {
          beatAvg += rates[x];
        }
        beatAvg /= RATE_SIZE;
        state.heartRate = beatAvg > 0 ? beatAvg : beatsPerMinute;
      }

      Serial.print("BPM: ");
      Serial.print(beatsPerMinute);
      Serial.print("  Avg BPM: ");
      Serial.println(beatAvg);
    }

    if (redValue > 0 && irValue > 0) {
      float ratio = (float)redValue / (float)irValue;
      state.spo2 = 104 - 17 * ratio;
      state.spo2 = constrain(state.spo2, 90, 100);
    }
  } else {
    state.heartRate = 0;
    state.spo2 = 0;
    beatAvg = 0;
    rateSpot = 0;
    lastBeat = 0;
    memset(rates, 0, sizeof(rates));
  }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n╔══════════════════════════════════════╗");
  Serial.println("║  Women Safety Device - Initializing  ║");
  Serial.println("╚══════════════════════════════════════╝\n");

  // Reset state before any module initializes it.
  memset(&state, 0, sizeof(DeviceState));
  state.heartRate = 0.0;
  state.spo2 = 0.0;
  state.latitude = 0.0;
  state.longitude = 0.0;

  // Initialize pins
  initializePins();
  
  // Startup LED sequence
  startupLEDSequence();
  
  // Initialize I2C
  Wire.begin(MAX30102_SDA_PIN, MAX30102_SCL_PIN);

  // Initialize DHT11 temperature sensor
  dht.begin();
  Serial.println("DHT11 body temperature sensor initialized on GPIO33");
  
  // Initialize sensors
  initMAX30102();
  // Initialize serial communications
  gpsSerial.begin(115200, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  gsmSerial.begin(9600, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);
  
  // Initialize WiFi
  initWiFi();
  
  // Initialize GSM
  initGSM();
  
  // Initialize VC-02 voice recognition module
  initVC02();
  
  // Load preferences
  preferences.begin("safety_dev", false);
  
  Serial.println("\n✅ Device Ready! All systems operational.\n");
  
  // Success indicator
  setLED(0, 255, 0);
  delay(1000);
  setLED(0, 0, 0);
  
  // Play startup beep
  playBeep(3, 100);
}

// ============================================================
// MAIN LOOP
// ============================================================
void serviceFastSensors(unsigned long durationMs) {
  unsigned long start = millis();
  do {
    readVibrationSensor();
    readHeartbeatSensor();
    readVC02();
    checkSOSButton();
    delay(1);
  } while (millis() - start < durationMs);
}

void loop() {
  unsigned long currentMillis = millis();
  state.systemUptime = currentMillis;

  // --- Read all sensors ---
  readVibrationSensor();
  readHeartbeatSensor();
  readBodyTemperature();
  readGPS();
  readVC02();
  checkSOSButton();
  readBatteryLevel();
  
  // --- Maintain WiFi connection ---
  maintainWiFi(currentMillis);
  
  // --- Process emergency logic ---
  processEmergencyLogic(currentMillis);
  
  // --- Send data to server ---
  if (currentMillis - state.lastDataSend >= DATA_SEND_INTERVAL) {
    sendDataToServer();
    state.lastDataSend = currentMillis;
  }
  
  // --- Check geo-fence ---
  checkGeoFence();
  
  // --- Update status LED ---
  updateStatusLED();
  
  // --- Serial debug output ---
  printDebugInfo(currentMillis);

  serviceFastSensors(20);
}

// ============================================================
// PIN INITIALIZATION
// ============================================================
void initializePins() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(SOS_BUTTON_PIN, INPUT_PULLUP);
  pinMode(VIBRATION_PIN, INPUT);
  pinMode(BATTERY_ADC_PIN, INPUT);
  
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_BLUE_PIN, LOW);
  
  Serial.println("✅ Pins initialized");
}

// ============================================================
// MAX30102 HEARTBEAT + SpO2 SENSOR
// ============================================================
bool isI2CDevicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void scanI2CBus() {
  byte devicesFound = 0;

  Serial.println("Scanning I2C bus...");
  for (uint8_t address = 1; address < 127; address++) {
    if (isI2CDevicePresent(address)) {
      Serial.printf("I2C device found at 0x%02X\n", address);
      devicesFound++;
    }
  }

  if (devicesFound == 0) {
    Serial.println("No I2C devices found. Check MAX30102 VCC, GND, SDA=GPIO21 and SCL=GPIO22.");
  }
}

void initMAX30102() {
  scanI2CBus();

  if (!isI2CDevicePresent(MAX30102_I2C_ADDR)) {
    Serial.println("MAX30102 not detected at address 0x57. Check wiring, 3.3V power, GND, and I2C pullups.");
    max30102Ready = false;
    blinkError(LED_RED_PIN, 3);
    return;
  }

  Serial.println("MAX30102 detected at address 0x57.");

  if (!particleSensor.begin()) {
    Serial.println("MAX30102 detected on I2C, but library initialization failed.");
    max30102Ready = false;
    blinkError(LED_RED_PIN, 3);
  } else {
    max30102Ready = true;
    Wire.setClock(I2C_SPEED_STANDARD);
    particleSensor.setup(
      MAX30102_LED_POWER,
      MAX30102_SAMPLE_AVERAGE,
      MAX30102_LED_MODE,
      MAX30102_SAMPLE_RATE,
      MAX30102_PULSE_WIDTH,
      MAX30102_ADC_RANGE
    );
    particleSensor.setPulseAmplitudeRed(MAX30102_LED_POWER);
    particleSensor.setPulseAmplitudeIR(MAX30102_LED_POWER);
    particleSensor.setPulseAmplitudeGreen(0);
    particleSensor.clearFIFO();
    Serial.println("MAX30102 heart rate sensor initialized. Place your finger on sensor.");
  }
}

void readHeartbeatSensor() {
  static unsigned long lastHeartDebug = 0;
  static long lastIrValue = 0;
  static long lastRedValue = 0;

  if (!max30102Ready) {
    state.heartRate = 0;
    state.spo2 = 0;
    return;
  }

  long irValue = particleSensor.getIR();
  long redValue = particleSensor.getRed();
  if (irValue == 0 && redValue == 0) return;
  lastIrValue = irValue;
  lastRedValue = redValue;
  addHeartRateSample(irValue, redValue);

  if (lastIrValue > MAX30102_FINGER_THRESHOLD) {
    if (millis() - lastHeartDebug > 2000) {
      lastHeartDebug = millis();
      Serial.printf("MAX30102 IR=%ld RED=%ld HR=%.0f BPM SpO2=%.1f%%\n",
                    lastIrValue, lastRedValue, state.heartRate, state.spo2);
    }
  } else {
    if (millis() - lastHeartDebug > 3000) {
      lastHeartDebug = millis();
      Serial.printf("MAX30102 waiting for finger. IR=%ld RED=%ld\n", lastIrValue, lastRedValue);
    }
  }
}

// ============================================================
// DHT11 BODY TEMPERATURE SENSOR
// ============================================================
void readBodyTemperature() {
  static unsigned long lastTempRead = 0;
  static unsigned long lastTempDebug = 0;

  if (millis() - lastTempRead < BODY_TEMP_UPDATE_INTERVAL) return;
  lastTempRead = millis();

  float temperature = dht.readTemperature();

  if (isnan(temperature)) {
    if (millis() - lastTempDebug > 5000) {
      lastTempDebug = millis();
      Serial.println("DHT11 temperature read failed. Check DATA=GPIO33, VCC, GND, and pull-up resistor.");
    }
    return;
  }

  state.temperature = temperature;

  if (millis() - lastTempDebug > 5000) {
    lastTempDebug = millis();
    Serial.printf("DHT11 body temperature: %.1f C\n", state.temperature);
  }
}
// ============================================================
// VIBRATION SENSOR (SW-420)
// ============================================================
void readVibrationSensor() {
  static bool calibrated = false;
  static bool idleLevel = LOW;
  static unsigned long calibrationStart = 0;
  static uint16_t highSamples = 0;
  static uint16_t lowSamples = 0;
  static unsigned long lastVibrationDebug = 0;
  static bool lastRawActive = false;
  static bool pollStableActive = false;
  static unsigned long lastRawChange = 0;

  unsigned long now = millis();
  bool rawLevel = digitalRead(VIBRATION_PIN);

  if (!calibrated) {
    if (calibrationStart == 0) {
      calibrationStart = now;
      Serial.println("SW-420 calibrating idle level. Keep vibration sensor still...");
    }

    if (rawLevel == HIGH) highSamples++;
    else lowSamples++;

    if (now - calibrationStart < VIBRATION_IDLE_CALIBRATION_MS) {
      return;
    }

    idleLevel = highSamples >= lowSamples ? HIGH : LOW;
    vibrationIdleLevel = idleLevel;
    vibrationLastInterruptMicros = micros();
    vibrationInterruptReady = true;
    attachInterrupt(digitalPinToInterrupt(VIBRATION_PIN), handleVibrationInterrupt, CHANGE);
    calibrated = true;
    lastRawActive = false;
    pollStableActive = false;
    lastRawChange = now;
    Serial.printf("SW-420 idle level calibrated: raw idle=%d. Interrupt counting enabled.\n",
                  idleLevel);
    return;
  }

  bool rawActive = rawLevel != idleLevel;

  if (rawActive != lastRawActive) {
    lastRawActive = rawActive;
    lastRawChange = now;
  }

  noInterrupts();
  uint16_t detectedEdges = vibrationPendingEdges;
  vibrationPendingEdges = 0;
  interrupts();

  if (now - lastRawChange >= VIBRATION_DEBOUNCE_MS && rawActive != pollStableActive) {
    pollStableActive = rawActive;
    if (pollStableActive && detectedEdges == 0) {
      detectedEdges = 1;
      Serial.println("SW-420 polling fallback detected vibration.");
    }
  }

  if (detectedEdges > 0) {
    for (uint16_t i = 0; i < detectedEdges; i++) {
      if (now - vibrationWindowStart <= VIBRATION_WINDOW_MS) {
        vibrationWindowCount++;
      } else {
        vibrationWindowCount = 1;
        vibrationWindowStart = now;
      }
    }
    
    state.vibrationCount = vibrationWindowCount;
    state.lastVibrationTime = now;
    
    Serial.printf("📳 Vibration detected! Count: %d\n", vibrationWindowCount);
  }
  
  if (vibrationWindowCount > 0 && now - vibrationWindowStart > VIBRATION_WINDOW_MS) {
    vibrationWindowCount = 0;
    state.vibrationCount = 0;
  }

  if (now - lastVibrationDebug > 2000) {
    lastVibrationDebug = now;
    Serial.printf("SW-420 raw=%d idle=%d active=%d count=%d\n",
                  rawLevel, idleLevel, rawActive, state.vibrationCount);
  }
}

// ============================================================
// GPS (NEO-6M)
// ============================================================
void readGPS() {
  static unsigned long lastGPSRead = 0;
  if (millis() - lastGPSRead < 100) return;
  lastGPSRead = millis();

  while (gpsSerial.available() > 0) {
    if (gps.encode(gpsSerial.read())) {
      if (gps.location.isUpdated()) {
        state.latitude = gps.location.lat();
        state.longitude = gps.location.lng();
        state.altitude = gps.altitude.meters();
        state.speed = gps.speed.kmph();
        state.gpsFixed = true;
        
        Serial.printf("📍 GPS: %.6f, %.6f | Speed: %.1f km/h\n",
          state.latitude, state.longitude, state.speed);
      }
    }
  }
  
  // Check if GPS signal lost
  if (millis() > 5000 && gps.charsProcessed() < 10) {
    state.gpsFixed = false;
  }
}

// ============================================================
// VC-02 VOICE RECOGNITION MODULE
// ============================================================
void initVC02() {
  pinMode(VC02_RX_PIN, INPUT_PULLUP);
  vc02Serial.begin(VC02_BAUD_RATE);
  Serial.printf("VC-02 voice module initialized on ESP32 RX=GPIO%d TX=GPIO%d @ %d baud\n",
                VC02_RX_PIN, VC02_TX_PIN, VC02_BAUD_RATE);
  Serial.printf("VC-02 wiring: module TX -> GPIO%d, module RX -> GPIO%d, common GND\n",
                VC02_RX_PIN, VC02_TX_PIN);
  Serial.println("VC-02 emergency command IDs:");
  Serial.printf("  0x%02X = help me\n", VC02_HELP_ME_COMMAND);
  Serial.printf("  0x%02X = emergency\n", VC02_EMERGENCY_COMMAND);
  Serial.printf("  0x%02X = save me\n", VC02_SAVE_ME_COMMAND);
  Serial.printf("  0x%02X = danger\n", VC02_DANGER_COMMAND);
  Serial.printf("  0x%02X = call police\n", VC02_CALL_POLICE_COMMAND);
  Serial.println("VC-02 check: speak the trained command and watch for a non-repeating command byte.");
}

bool isVC02EmergencyCommand(uint8_t command) {
  return command == VC02_HELP_ME_COMMAND ||
         command == VC02_EMERGENCY_COMMAND ||
         command == VC02_SAVE_ME_COMMAND ||
         command == VC02_DANGER_COMMAND ||
         command == VC02_CALL_POLICE_COMMAND;
}

const char* getVC02CommandName(uint8_t command) {
  switch (command) {
    case VC02_HELP_ME_COMMAND:     return "Help Me";
    case VC02_EMERGENCY_COMMAND:   return "Emergency";
    case VC02_SAVE_ME_COMMAND:     return "Save Me";
    case VC02_DANGER_COMMAND:      return "Danger";
    case VC02_CALL_POLICE_COMMAND: return "Call Police";
    default:                       return "Unknown Voice Command";
  }
}

void handleVC02Command(uint8_t command) {
  if (command == 0x00 || command == 0xFF) {
    return;
  }

  state.lastVoiceCommand = command;
  Serial.printf("VC-02 voice command received: 0x%02X (%s)\n",
                command, getVC02CommandName(command));

  if (isVC02EmergencyCommand(command)) {
    state.voiceCommandDetected = true;
  }
}

void handleVC02Byte(uint8_t incoming) {
  static unsigned long lastZeroWarn = 0;
  static unsigned long lastRepeatWarn = 0;
  static uint8_t lastIncoming = 0xFF;
  static uint8_t repeatCount = 0;

  state.lastVoiceByte = incoming;

  if (VC02_RAW_DEBUG) {
    Serial.printf("VC-02 RX byte: 0x%02X\n", incoming);
  }

  if (incoming == lastIncoming) {
    if (repeatCount < 255) {
      repeatCount++;
    }
  } else {
    lastIncoming = incoming;
    repeatCount = 1;
  }

  if (incoming == 0x00) {
    if (millis() - lastZeroWarn > VC02_ZERO_WARN_MS) {
      lastZeroWarn = millis();
      Serial.println("VC-02 received 0x00 only. This is not a command; check UART TX pin, idle voltage, training, and serial-output setting.");
    }
    return;
  }

  if (incoming == 0xFF) {
    return;
  }

  if (!isVC02EmergencyCommand(incoming) &&
      repeatCount >= VC02_REPEAT_WARN_COUNT &&
      millis() - lastRepeatWarn > VC02_REPEAT_WARN_MS) {
    lastRepeatWarn = millis();
    Serial.printf("VC-02 repeating 0x%02X. If this prints without speaking, it is probably UART noise/wrong data. Check VC-02 TX -> GPIO%d, RX -> GPIO%d, common GND, baud rate, and serial-output mode.\n",
                  incoming, VC02_RX_PIN, VC02_TX_PIN);
  }

  if (isVC02EmergencyCommand(incoming)) {
    handleVC02Command(incoming);
  }
}

void readVC02() {
  static uint8_t frame[VC02_FRAME_LENGTH];
  static uint8_t index = 0;
  static unsigned long lastByteAt = 0;

  if (index > 0 && millis() - lastByteAt > VC02_FRAME_TIMEOUT_MS) {
    index = 0;
  }

  while (vc02Serial.available() > 0) {
    uint8_t incoming = vc02Serial.read();
    lastByteAt = millis();
    handleVC02Byte(incoming);

    if (index == 0 && incoming != VC02_FRAME_HEADER) {
      continue;
    }

    frame[index++] = incoming;

    if (index >= VC02_FRAME_LENGTH) {
      Serial.printf("VC-02 frame: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
                    frame[0], frame[1], frame[2], frame[3], frame[4]);
      for (uint8_t i = 1; i < VC02_FRAME_LENGTH; i++) {
        handleVC02Command(frame[i]);
      }
      index = 0;
    }
  }
}
// ============================================================
// SOS BUTTON
// ============================================================
void checkSOSButton() {
  static bool lastButtonState = HIGH;
  static unsigned long buttonPressTime = 0;
  static int pressCount = 0;
  static unsigned long lastPressTime = 0;
  
  bool currentState = digitalRead(SOS_BUTTON_PIN);
  
  // Button pressed (active LOW)
  if (currentState == LOW && lastButtonState == HIGH) {
    buttonPressTime = millis();
    
    // Count rapid presses for SOS (3 presses = SOS)
    if (millis() - lastPressTime < 1000) {
      pressCount++;
    } else {
      pressCount = 1;
    }
    lastPressTime = millis();
    
    if (pressCount >= 3) {
      Serial.println("🚨 SOS BUTTON TRIPLE PRESS!");
      triggerEmergencyAlert(ALERT_SOS_BUTTON, "SOS Button Pressed (3x)");
      pressCount = 0;
    }
    
    // Long press (3 seconds) = immediate emergency
    Serial.println("🔴 SOS Button pressed...");
  }
  
  // Long press detection
  if (currentState == LOW && buttonPressTime > 0) {
    if (millis() - buttonPressTime >= 3000) {
      Serial.println("🚨 LONG PRESS SOS!");
      triggerEmergencyAlert(ALERT_SOS_BUTTON, "SOS Long Press");
      buttonPressTime = 0;
    }
  }
  
  lastButtonState = currentState;
}

// ============================================================
// EMERGENCY ALERT SYSTEM
// ============================================================
void processEmergencyLogic(unsigned long currentMillis) {
  // Cooldown check
  if (state.emergencyActive && 
      currentMillis - state.lastAlertTime_ms > ALERT_COOLDOWN_MS) {
    // Auto-cancel after cooldown if no manual cancel
    // state.emergencyActive = false;
  }
  
  // Trigger from heart rate anomaly
  if (state.heartRate > 0) {
    if (state.heartRate > HEART_RATE_HIGH_THRESHOLD) {
      if (currentMillis - state.lastAlertTime_ms > ALERT_COOLDOWN_MS) {
        Serial.printf("⚠️  High heart rate: %.0f BPM\n", state.heartRate);
        triggerEmergencyAlert(ALERT_HEART_RATE_HIGH, "High Heart Rate");
      }
    }
    if (state.heartRate < HEART_RATE_LOW_THRESHOLD) {
      if (currentMillis - state.lastAlertTime_ms > ALERT_COOLDOWN_MS) {
        Serial.printf("⚠️  Low heart rate: %.0f BPM\n", state.heartRate);
        triggerEmergencyAlert(ALERT_HEART_RATE_LOW, "Low Heart Rate");
      }
    }
  }
  
  // Trigger from excessive vibration (attack/struggle)
  if (state.vibrationCount >= VIBRATION_COUNT_THRESHOLD) {
    if (currentMillis - state.lastAlertTime_ms > ALERT_COOLDOWN_MS) {
      triggerEmergencyAlert(ALERT_VIBRATION, "Excessive Vibration/Struggle");
      vibrationWindowCount = 0;
      state.vibrationCount = 0;
    }
  }

  // Trigger from VC-02 emergency voice command
  if (state.voiceCommandDetected) {
    if (currentMillis - state.lastAlertTime_ms > ALERT_COOLDOWN_MS) {
      String voiceReason = "VC-02 Voice Command: ";
      voiceReason += getVC02CommandName(state.lastVoiceCommand);
      triggerEmergencyAlert(ALERT_VOICE_COMMAND, voiceReason);
    }
    state.voiceCommandDetected = false;
  }
  
  // Keep buzzer going if emergency active
  if (state.emergencyActive) {
    buzzAlert();
    setLED(255, 0, 0);  // Red LED
  }
}

void triggerEmergencyAlert(int alertType, String reason) {
  if (state.emergencyActive && 
      millis() - state.lastAlertTime_ms < 5000) return;  // Debounce

  Serial.println("\n🚨🚨🚨 EMERGENCY ALERT TRIGGERED! 🚨🚨🚨");
  Serial.printf("Type: %d | Reason: %s\n", alertType, reason.c_str());
  
  state.emergencyActive = true;
  state.alertType = alertType;
  state.alertSent = false;
  state.lastAlertTime_ms = millis();
  
  // Immediate buzzer
  buzzSOS();
  
  // Flash red LED
  setLED(255, 0, 0);
  
  // Send SMS to all contacts
  for (int i = 0; i < 3; i++) {
    if (emergencyContacts[i].length() > 3) {
      sendEmergencySMS(emergencyContacts[i], alertType, reason);
      delay(500);
    }
  }
  
  // Make emergency call
  if (emergencyContacts[0].length() > 3) {
    makeEmergencyCall(emergencyContacts[0]);
  }
  
  // Send to server
  sendEmergencyToServer(alertType, reason);
  
  state.alertSent = true;
  Serial.println("✅ Emergency alert sent to all contacts!\n");
}

void cancelAlert() {
  state.emergencyActive = false;
  state.alertType = ALERT_NONE;
  digitalWrite(BUZZER_PIN, LOW);
  setLED(0, 255, 0);
  Serial.println("✅ Alert cancelled by user");
  
  // Notify server
  sendCancelToServer();
}

// ============================================================
// GSM COMMUNICATION (SIM800L)
// ============================================================
void initGSM() {
  Serial.println("Initializing GSM...");
  delay(3000);
  
  sendGSMCommand("AT", 1000);
  sendGSMCommand("AT+CMGF=1", 1000);       // SMS text mode
  sendGSMCommand("AT+CNMI=2,2,0,0,0", 1000); // Auto SMS
  
  String response = sendGSMCommand("AT+CSQ", 2000);
  if (response.indexOf("OK") >= 0) {
    state.gsmConnected = true;
    Serial.println("✅ GSM SIM800L initialized");
  } else {
    Serial.println("❌ GSM initialization failed");
  }
}

void sendEmergencySMS(String phoneNumber, int alertType, String reason) {
  String alertName = getAlertTypeName(alertType);
  String locationStr = "";
  
  if (state.gpsFixed) {
    locationStr = "https://maps.google.com/?q=" + 
                  String(state.latitude, 6) + "," + 
                  String(state.longitude, 6);
  } else {
    locationStr = "GPS signal unavailable";
  }
  
  String message = "🚨 EMERGENCY ALERT!\n";
  message += "Alert: " + alertName + "\n";
  message += "Reason: " + reason + "\n";
  message += "Heart Rate: " + String((int)state.heartRate) + " BPM\n";
  message += "Battery: " + String(state.batteryLevel) + "%\n";
  message += "Location: " + locationStr + "\n";
  message += "Time: " + String(millis()/1000) + "s uptime\n";
  message += "Device ID: " + DEVICE_ID;
  
  Serial.printf("📱 Sending SMS to %s\n", phoneNumber.c_str());
  
  gsmSerial.println("AT+CMGF=1");
  delay(200);
  gsmSerial.println("AT+CMGS=\"" + phoneNumber + "\"");
  delay(200);
  gsmSerial.println(message);
  delay(200);
  gsmSerial.write((uint8_t)26);  // Ctrl+Z to send
  delay(3000);
  
  Serial.println("✅ SMS sent!");
}

void sendLocationSMS() {
  for (int i = 0; i < 3; i++) {
    if (emergencyContacts[i].length() > 3) {
      String locationStr = "";
      if (state.gpsFixed) {
        locationStr = "📍 Current Location:\nhttps://maps.google.com/?q=" + 
                      String(state.latitude, 6) + "," + 
                      String(state.longitude, 6);
      } else {
        locationStr = "GPS signal not available";
      }
      
      gsmSerial.println("AT+CMGF=1");
      delay(200);
      gsmSerial.println("AT+CMGS=\"" + emergencyContacts[i] + "\"");
      delay(200);
      gsmSerial.println("Safety Device Location Update:\n" + locationStr);
      delay(200);
      gsmSerial.write((uint8_t)26);
      delay(3000);
    }
  }
}

void makeEmergencyCall(String phoneNumber) {
  Serial.printf("📞 Calling %s\n", phoneNumber.c_str());
  gsmSerial.println("ATD" + phoneNumber + ";");
  delay(20000);  // Call for 20 seconds
  gsmSerial.println("ATH");  // Hang up
  delay(1000);
}

String sendGSMCommand(String command, int timeout) {
  gsmSerial.println(command);
  unsigned long start = millis();
  String response = "";
  
  while (millis() - start < timeout) {
    while (gsmSerial.available()) {
      response += (char)gsmSerial.read();
    }
  }
  
  Serial.printf("GSM [%s] -> %s\n", command.c_str(), response.c_str());
  return response;
}

// ============================================================
// WIFI & HTTP COMMUNICATION
// ============================================================
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    state.wifiConnected = true;
    Serial.printf("\n✅ WiFi Connected! IP: %s\n", 
                  WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n⚠️  WiFi failed, using GSM only");
  }
}

void maintainWiFi(unsigned long currentMillis) {
  static unsigned long lastWiFiCheck = 0;
  
  if (currentMillis - lastWiFiCheck >= WIFI_RECONNECT_INTERVAL) {
    lastWiFiCheck = currentMillis;
    
    if (WiFi.status() != WL_CONNECTED) {
      state.wifiConnected = false;
      Serial.println("⚠️  WiFi disconnected, reconnecting...");
      WiFi.reconnect();
    } else {
      state.wifiConnected = true;
    }
  }
}

void sendDataToServer() {
  if (!state.wifiConnected) return;
  
  HTTPClient http;
  http.begin(SERVER_URL + "update_device.php");
  http.addHeader("Content-Type", "application/json");
  
  // Build JSON payload
  StaticJsonDocument<512> doc;
  doc["device_id"]       = DEVICE_ID;
  doc["latitude"]        = state.latitude;
  doc["longitude"]       = state.longitude;
  doc["altitude"]        = state.altitude;
  doc["speed"]           = state.speed;
  doc["heart_rate"]      = state.heartRate;
  doc["spo2"]            = state.spo2;
  doc["temperature"]     = state.temperature;
  doc["battery"]         = state.batteryLevel;
  doc["vibration_count"] = state.vibrationCount;
  doc["voice_command"]   = state.lastVoiceCommand;
  doc["gps_fixed"]       = state.gpsFixed;
  doc["wifi_connected"]  = state.wifiConnected;
  doc["gsm_connected"]   = state.gsmConnected;
  doc["emergency_active"]= state.emergencyActive;
  doc["alert_type"]      = state.alertType;
  
  String jsonStr;
  serializeJson(doc, jsonStr);
  
  int httpCode = http.POST(jsonStr);
  
  if (httpCode == 200) {
    String response = http.getString();
    Serial.printf("✅ Data sent to server. Response: %s\n", response.c_str());
    
    // Check for server commands
    StaticJsonDocument<256> respDoc;
    deserializeJson(respDoc, response);
    
    JsonVariant data = respDoc["data"];
    const char* command = nullptr;
    if (!data.isNull() && data["command"].is<const char*>()) {
      command = data["command"];
    } else if (respDoc["command"].is<const char*>()) {
      command = respDoc["command"];
    }

    if (command) {
      String cmd = command;
      if (cmd == "cancel_alert") cancelAlert();
      if (cmd == "trigger_buzzer") buzzAlert();
    }
  } else {
    Serial.printf("❌ HTTP Error: %d\n", httpCode);
  }
  
  http.end();
}

void sendEmergencyToServer(int alertType, String reason) {
  if (!state.wifiConnected) return;
  
  HTTPClient http;
  http.begin(SERVER_URL + "emergency_alert.php");
  http.addHeader("Content-Type", "application/json");
  
  StaticJsonDocument<512> doc;
  doc["device_id"]    = DEVICE_ID;
  doc["alert_type"]   = alertType;
  doc["alert_name"]   = getAlertTypeName(alertType);
  doc["reason"]       = reason;
  doc["latitude"]     = state.latitude;
  doc["longitude"]    = state.longitude;
  doc["heart_rate"]   = state.heartRate;
  doc["spo2"]         = state.spo2;
  doc["battery"]      = state.batteryLevel;
  doc["gps_fixed"]    = state.gpsFixed;
  
  String jsonStr;
  serializeJson(doc, jsonStr);
  
  int httpCode = http.POST(jsonStr);
  Serial.printf("Emergency sent to server: %d\n", httpCode);
  http.end();
}

void sendCancelToServer() {
  if (!state.wifiConnected) return;
  
  HTTPClient http;
  http.begin(SERVER_URL + "cancel_alert.php");
  http.addHeader("Content-Type", "application/json");
  
  StaticJsonDocument<128> doc;
  doc["device_id"] = DEVICE_ID;
  doc["action"]    = "cancel";
  
  String jsonStr;
  serializeJson(doc, jsonStr);
  http.POST(jsonStr);
  http.end();
}

// ============================================================
// BATTERY MONITORING
// ============================================================
void readBatteryLevel() {
  static unsigned long lastBatRead = 0;
  if (millis() - lastBatRead < 5000) return;
  lastBatRead = millis();
  
  int rawADC = analogRead(BATTERY_ADC_PIN);
  float voltage = (rawADC / 4095.0) * 3.3 * 2;  // Voltage divider
  
  // 3.7V LiPo: 4.2V = 100%, 3.2V = 0%
  state.batteryLevel = map(voltage * 100, 320, 420, 0, 100);
  state.batteryLevel = constrain(state.batteryLevel, 0, 100);
  
  if (state.batteryLevel < 10) {
    Serial.println("⚠️  Battery critically low!");
    blinkError(LED_RED_PIN, 5);
  }
}

// ============================================================
// GEO-FENCE CHECK
// ============================================================
void checkGeoFence() {
  // Load saved geofence from preferences
  float safeLat  = preferences.getFloat("geo_lat", 0.0);
  float safeLon  = preferences.getFloat("geo_lon", 0.0);
  float safeRadius = preferences.getFloat("geo_rad", 500.0); // meters
  
  if (safeLat == 0.0 || !state.gpsFixed) return;
  
  // Haversine distance calculation
  float dist = haversineDistance(state.latitude, state.longitude, 
                                  safeLat, safeLon);
  
  if (dist > safeRadius) {
    Serial.printf("⚠️  GeoFence breach! Distance: %.0f m\n", dist);
    if (millis() - state.lastAlertTime_ms > ALERT_COOLDOWN_MS) {
      triggerEmergencyAlert(ALERT_GEOFENCE, "GeoFence Boundary Exceeded");
    }
  }
}

float haversineDistance(float lat1, float lon1, float lat2, float lon2) {
  const float R = 6371000;  // Earth radius in meters
  float dLat = (lat2 - lat1) * PI / 180;
  float dLon = (lon2 - lon1) * PI / 180;
  float a = sin(dLat/2) * sin(dLat/2) +
            cos(lat1 * PI / 180) * cos(lat2 * PI / 180) *
            sin(dLon/2) * sin(dLon/2);
  float c = 2 * atan2(sqrt(a), sqrt(1-a));
  return R * c;
}

// ============================================================
// BUZZER PATTERNS
// ============================================================
void buzzSOS() {
  // SOS: ... --- ...
  int dotDuration = 150;
  int dashDuration = 450;
  
  for (int i = 0; i < 3; i++) { // Dots
    digitalWrite(BUZZER_PIN, HIGH); delay(dotDuration);
    digitalWrite(BUZZER_PIN, LOW);  delay(dotDuration);
  }
  delay(200);
  for (int i = 0; i < 3; i++) { // Dashes
    digitalWrite(BUZZER_PIN, HIGH); delay(dashDuration);
    digitalWrite(BUZZER_PIN, LOW);  delay(dotDuration);
  }
  delay(200);
  for (int i = 0; i < 3; i++) { // Dots
    digitalWrite(BUZZER_PIN, HIGH); delay(dotDuration);
    digitalWrite(BUZZER_PIN, LOW);  delay(dotDuration);
  }
}

void buzzAlert() {
  static unsigned long lastBuzz = 0;
  if (millis() - lastBuzz > 2000) {
    lastBuzz = millis();
    for (int i = 0; i < 3; i++) {
      digitalWrite(BUZZER_PIN, HIGH); delay(300);
      digitalWrite(BUZZER_PIN, LOW);  delay(200);
    }
  }
}

void playBeep(int count, int duration) {
  for (int i = 0; i < count; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
    delay(duration);
  }
}

// ============================================================
// LED CONTROL
// ============================================================
void setLED(int r, int g, int b) {
  analogWrite(LED_RED_PIN, r);
  analogWrite(LED_GREEN_PIN, g);
  analogWrite(LED_BLUE_PIN, b);
}

void updateStatusLED() {
  static unsigned long lastLEDBlink = 0;
  static bool ledState = false;
  unsigned long now = millis();
  
  if (state.emergencyActive) {
    // Fast red blink
    if (now - lastLEDBlink > 250) {
      ledState = !ledState;
      setLED(ledState ? 255 : 0, 0, 0);
      lastLEDBlink = now;
    }
  } else if (!state.gpsFixed) {
    // Yellow blink (waiting for GPS)
    if (now - lastLEDBlink > 1000) {
      ledState = !ledState;
      setLED(ledState ? 255 : 0, ledState ? 165 : 0, 0);
      lastLEDBlink = now;
    }
  } else if (state.wifiConnected) {
    // Slow green blink (all good)
    if (now - lastLEDBlink > 2000) {
      ledState = !ledState;
      setLED(0, ledState ? 100 : 0, 0);
      lastLEDBlink = now;
    }
  } else {
    // Blue blink (GSM only)
    if (now - lastLEDBlink > 1500) {
      ledState = !ledState;
      setLED(0, 0, ledState ? 100 : 0);
      lastLEDBlink = now;
    }
  }
}

void startupLEDSequence() {
  setLED(255, 0, 0); delay(300);
  setLED(0, 255, 0); delay(300);
  setLED(0, 0, 255); delay(300);
  setLED(255, 255, 0); delay(300);
  setLED(0, 0, 0);
}

void blinkError(int pin, int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH); delay(200);
    digitalWrite(pin, LOW);  delay(200);
  }
}

// ============================================================
// ANNOUNCE STATUS (buzzer pattern)
// ============================================================
void announceStatus() {
  if (state.wifiConnected) playBeep(2, 100);
  if (state.gpsFixed) playBeep(3, 100);
  if (state.heartRate > 0) playBeep(1, 500);
}

// ============================================================
// HELPER FUNCTIONS
// ============================================================
String getAlertTypeName(int type) {
  switch (type) {
    case ALERT_SOS_BUTTON:      return "SOS Button Pressed";
    case ALERT_VOICE_COMMAND:   return "Voice Command";
    case ALERT_HEART_RATE_HIGH: return "High Heart Rate";
    case ALERT_HEART_RATE_LOW:  return "Low Heart Rate";
    case ALERT_VIBRATION:       return "Excessive Vibration";
    case ALERT_GEOFENCE:        return "GeoFence Breach";
    default:                    return "Unknown Alert";
  }
}

void printDebugInfo(unsigned long currentMillis) {
  static unsigned long lastDebug = 0;
  if (currentMillis - lastDebug < 5000) return;
  lastDebug = currentMillis;
  
  Serial.println("\n═══════════════════════════════════════");
  Serial.printf("📊 Device Status | Uptime: %lus\n", currentMillis/1000);
  Serial.println("═══════════════════════════════════════");
  Serial.printf("❤️  Heart Rate : %.0f BPM | SpO2: %.1f%%\n", 
                state.heartRate, state.spo2);
  Serial.printf("📍 GPS        : %s | %.6f, %.6f\n",
                state.gpsFixed ? "FIXED" : "NO FIX",
                state.latitude, state.longitude);
  Serial.printf("Voice Cmd  : 0x%02X | Last VC-02 byte: 0x%02X | Temp: %.1f C\n",
                state.lastVoiceCommand, state.lastVoiceByte, state.temperature);
  Serial.printf("📳 Vibration  : %d counts\n", state.vibrationCount);
  Serial.printf("🔋 Battery    : %d%%\n", state.batteryLevel);
  Serial.printf("📶 WiFi       : %s | GSM: %s\n",
                state.wifiConnected ? "ON" : "OFF",
                state.gsmConnected ? "ON" : "OFF");
  Serial.printf("🚨 Emergency  : %s | Type: %d\n",
                state.emergencyActive ? "ACTIVE" : "None",
                state.alertType);
  Serial.println("═══════════════════════════════════════\n");
}
