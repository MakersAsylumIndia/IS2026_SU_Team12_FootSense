/*
  =====================================================================
  SMART INSOLE - FINAL FIRMWARE
  3x Flex Sensor + 2x Piezo Disc + MPU6050 + WiFi Web Server
  =====================================================================
  Board : ESP32 DevKit V1 (38-pin, "WROOM-32" / DOIT)

  WIRING
  --------------------------------------------------------------------
  MPU6050 (I2C):
    VCC -> 3V3
    GND -> GND
    SDA -> GPIO21   (+ 10k ohm pull-up resistor to 3V3)
    SCL -> GPIO22   (+ 10k ohm pull-up resistor to 3V3)

  Flex Sensor 1 - Medial Arch (voltage divider):
    3V3 -- [Flex 1] --+-- GPIO34
                        |
                    [33k resistor] -- GND

  Flex Sensor 2 - Rear Arch (voltage divider):
    3V3 -- [Flex 2] --+-- GPIO35
                        |
                    [33k resistor] -- GND

  Flex Sensor 3 - Anterior Arch (voltage divider):
    3V3 -- [Flex 3] --+-- GPIO32
                        |
                    [33k resistor] -- GND

  Piezo 1 (damping resistor in parallel):
    Piezo1(+) --+-- GPIO4
                 |
             [1M resistor] -- GND (other piezo lead also to GND)

  Piezo 2 (damping resistor in parallel):
    Piezo2(+) --+-- GPIO33
                 |
             [1M resistor] -- GND (other piezo lead also to GND)

  NOTE: GPIO34/35/32/33 are all ADC1 channel pins - safe to use
  together with WiFi active (ADC2 pins conflict with WiFi and are
  deliberately avoided here).

  Libraries required (Arduino Library Manager):
    - "MPU6050_light" by rfetick
    - "ArduinoJson" by Benoit Blanchon
    (WiFi.h and WebServer.h are built into the ESP32 Arduino core)

  WHAT THIS FIRMWARE DOES
  --------------------------------------------------------------------
  1. Connects to your WiFi network and starts a small web server.
  2. Continuously samples all 6 sensors + MPU6050 at 50Hz.
  3. Runs a self-calibrating flex sensor baseline: at startup, it
     assumes a "rest" reading per sensor and tracks deviation from
     it - so it works correctly regardless of which way round your
     specific divider wiring biases the reading.
  4. Runs pedometer step counting from accelerometer magnitude.
  5. Detects piezo impacts and records the MPU6050 pitch/roll at the
     moment of impact (impact angle).
  6. Exposes a single JSON endpoint (/data) with live + aggregated
     stats, which the dashboard (separate HTML file) polls.
  7. Exposes a /reset endpoint to zero out step count and impact
     history for a fresh session.

  HOW TO USE
  --------------------------------------------------------------------
  1. Fill in your WiFi SSID and password below.
  2. Upload this sketch.
  3. Open Serial Monitor at 115200 baud - it will print the ESP32's
     IP address once connected.
  4. Open the dashboard HTML file (provided separately), enter that
     IP address in the settings field, and it will start polling
     live data automatically.
  =====================================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <MPU6050_light.h>
#include <ArduinoJson.h>

// ---------------------- WIFI CREDENTIALS ----------------------
const char* WIFI_SSID     = "hotspot";
const char* WIFI_PASSWORD = "password";

// ---------------------- PIN DEFINITIONS ----------------------
#define I2C_SDA      21
#define I2C_SCL      22

#define FLEX1_PIN    34   // Medial arch
#define FLEX2_PIN    35   // Rear arch
#define FLEX3_PIN    32   // Anterior arch

#define PIEZO1_PIN   4
#define PIEZO2_PIN   33

// ---------------------- OBJECTS ----------------------
MPU6050 mpu(Wire);
WebServer server(80);

// ---------------------- FLEX SENSOR STATE ----------------------
// Self-calibrating: we record a resting baseline per sensor at startup,
// then report DEVIATION from that baseline rather than assuming a fixed
// direction. This makes the firmware correct regardless of whether your
// physical divider wiring makes the reading rise or fall with bend.
int flexBaseline[3] = {0, 0, 0};
int flexRaw[3] = {0, 0, 0};
float flexBendPct[3] = {0, 0, 0}; // 0-100%, calibrated per-sensor below

// Calibration range: how much raw ADC swing we expect from flat to a
// firm bend. These are starting estimates - run the sketch, flex each
// sensor fully, and refine these per-sensor if needed.
const int FLEX_BEND_RANGE = 800; // raw ADC counts from flat to full bend

// Noise control: each reading is smoothed by averaging several quick
// back-to-back ADC samples, then a deadzone is applied so tiny ADC
// noise at rest is reported as exactly 0% instead of jittering.
const int FLEX_SMOOTHING_SAMPLES = 8;
const int FLEX_DEADZONE = 15; // raw ADC counts

// ---------------------- PIEZO STATE ----------------------
int piezoBaseline[2] = {2048, 2048};
// Separate threshold per piezo - piezos can vary sensor-to-sensor in how
// large a voltage spike they produce for the same physical tap, so a
// single shared threshold can work for one and miss the other entirely.
// PIEZO_IMPACT_THRESHOLD[0] = heel, [1] = forefoot.
int PIEZO_IMPACT_THRESHOLD[2] = {200, 80}; // <-- tune index 1 (forefoot) based on what you observed
const unsigned long PIEZO_DEBOUNCE_MS = 150;
unsigned long lastImpactTime[2] = {0, 0};
unsigned long impactCount[2] = {0, 0};

// Impact angle history - records pitch/roll at the moment of each impact
const int IMPACT_HISTORY_SIZE = 50;
float impactAngleHistory[IMPACT_HISTORY_SIZE];
int impactHistoryIndex = 0;
int impactHistoryCount = 0;

// ---------------------- PEDOMETER STATE ----------------------
const float STEP_THRESHOLD_HIGH = 1.35;
const float STEP_THRESHOLD_LOW  = 0.85;
const unsigned long STEP_DEBOUNCE_MS = 300;
unsigned long stepCount = 0;
bool stepArmed = true;
unsigned long lastStepTime = 0;

// ---------------------- SESSION STATS ----------------------
unsigned long sessionStartTime = 0;
float flexBendPctSum[3] = {0, 0, 0};
unsigned long flexSampleCount = 0;

// ---------------------- TIMING ----------------------
unsigned long lastSampleTime = 0;
const unsigned long SAMPLE_INTERVAL_MS = 20; // 50 Hz

// =====================================================================
// HELPER FUNCTIONS
// =====================================================================

// Reads a pin multiple times back-to-back and returns the average,
// which smooths out a lot of the ADC's natural sample-to-sample noise.
int readSmoothed(int pin) {
  long sum = 0;
  for (int i = 0; i < FLEX_SMOOTHING_SAMPLES; i++) {
    sum += analogRead(pin);
  }
  return sum / FLEX_SMOOTHING_SAMPLES;
}

// =====================================================================
// SETUP
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("====================================="));
  Serial.println(F(" Smart Insole - Final Firmware"));
  Serial.println(F("====================================="));

  // ---- MPU6050 ----
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  byte status = mpu.begin();
  Serial.print(F("MPU6050 status: "));
  Serial.println(status);
  while (status != 0) {
    Serial.println(F("MPU6050 connection failed! Check wiring + pull-ups."));
    delay(1000);
    status = mpu.begin();
  }

  Serial.println(F("Calibrating MPU6050 - keep insole flat and still..."));
  delay(1000);
  mpu.calcOffsets(true, true);
  Serial.println(F("MPU6050 calibration done."));

  // ---- Flex sensors ----
  analogReadResolution(12);
  pinMode(FLEX1_PIN, INPUT);
  pinMode(FLEX2_PIN, INPUT);
  pinMode(FLEX3_PIN, INPUT);

  Serial.println(F("Calibrating flex sensor baselines - keep insole flat, no weight..."));
  delay(500);
  long sums[3] = {0, 0, 0};
  const int calSamples = 50;
  for (int i = 0; i < calSamples; i++) {
    sums[0] += readSmoothed(FLEX1_PIN);
    sums[1] += readSmoothed(FLEX2_PIN);
    sums[2] += readSmoothed(FLEX3_PIN);
    delay(10);
  }
  for (int i = 0; i < 3; i++) flexBaseline[i] = sums[i] / calSamples;

  Serial.print(F("Flex baselines: "));
  Serial.print(flexBaseline[0]); Serial.print(", ");
  Serial.print(flexBaseline[1]); Serial.print(", ");
  Serial.println(flexBaseline[2]);

  // ---- Piezo sensors ----
  Serial.println(F("Calibrating piezo baselines - do not touch..."));
  delay(500);
  long psum[2] = {0, 0};
  for (int i = 0; i < calSamples; i++) {
    psum[0] += analogRead(PIEZO1_PIN);
    psum[1] += analogRead(PIEZO2_PIN);
    delay(10);
  }
  piezoBaseline[0] = psum[0] / calSamples;
  piezoBaseline[1] = psum[1] / calSamples;

  Serial.print(F("Piezo baselines: "));
  Serial.print(piezoBaseline[0]); Serial.print(", ");
  Serial.println(piezoBaseline[1]);

  // ---- WiFi ----
  Serial.print(F("Connecting to WiFi: "));
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 30) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print(F("WiFi connected! IP address: "));
    Serial.println(WiFi.localIP());
    Serial.println(F("Enter this IP into the dashboard's settings field."));
  } else {
    Serial.println();
    Serial.println(F("!!! WiFi connection FAILED !!!"));
    Serial.println(F("Check WIFI_SSID / WIFI_PASSWORD and try again."));
  }

  // ---- Web server routes ----
  server.on("/data", HTTP_GET, handleData);
  server.on("/reset", HTTP_GET, handleReset);
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found. Use /data or /reset.");
  });

  // Allow cross-origin requests so the dashboard HTML (opened as a local
  // file or hosted elsewhere) can fetch this data without being blocked.
  server.enableCORS(true);

  server.begin();
  Serial.println(F("Web server started."));

  sessionStartTime = millis();
  lastSampleTime = millis();
}

// =====================================================================
// MAIN LOOP
// =====================================================================
void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - lastSampleTime < SAMPLE_INTERVAL_MS) {
    return;
  }
  lastSampleTime = now;

  // ---- MPU6050 ----
  mpu.update();
  float accX = mpu.getAccX();
  float accY = mpu.getAccY();
  float accZ = mpu.getAccZ();
  float pitch = mpu.getAngleX();
  float roll  = mpu.getAngleY();

  float accMagnitude = sqrt(accX * accX + accY * accY + accZ * accZ);

  // ---- Pedometer ----
  if (stepArmed && accMagnitude > STEP_THRESHOLD_HIGH) {
    if (now - lastStepTime > STEP_DEBOUNCE_MS) {
      stepCount++;
      lastStepTime = now;
      stepArmed = false;
    }
  } else if (!stepArmed && accMagnitude < STEP_THRESHOLD_LOW) {
    stepArmed = true;
  }

  // ---- Flex sensors (self-calibrating deviation from baseline) ----
  flexRaw[0] = readSmoothed(FLEX1_PIN);
  flexRaw[1] = readSmoothed(FLEX2_PIN);
  flexRaw[2] = readSmoothed(FLEX3_PIN);

  for (int i = 0; i < 3; i++) {
    // Deviation from baseline, in EITHER direction, scaled to 0-100%.
    // This makes the firmware correct regardless of which way your
    // physical divider wiring biases the raw reading.
    float deviation = abs(flexRaw[i] - flexBaseline[i]);
    // Deadzone: tiny deviations from noise alone read as exactly 0%
    // instead of jittering, so the sensor reads a clean 0 at rest.
    if (deviation < FLEX_DEADZONE) deviation = 0;
    flexBendPct[i] = constrain((deviation / (float)FLEX_BEND_RANGE) * 100.0, 0, 100);
    flexBendPctSum[i] += flexBendPct[i];
  }
  flexSampleCount++;

  // ---- Piezo sensors + impact angle capture ----
  // NOTE: piezo readings are intentionally NOT smoothed/averaged like the
  // flex sensors. A piezo impact is a brief, sharp voltage spike - averaging
  // several samples together would blunt or even miss that spike entirely.
  // Raw single-sample reads are correct here.
  int piezoRaw[2];
  piezoRaw[0] = analogRead(PIEZO1_PIN);
  piezoRaw[1] = analogRead(PIEZO2_PIN);

  for (int i = 0; i < 2; i++) {
    int deviation = abs(piezoRaw[i] - piezoBaseline[i]);
    if (deviation > PIEZO_IMPACT_THRESHOLD[i] && (now - lastImpactTime[i] > PIEZO_DEBOUNCE_MS)) {
      lastImpactTime[i] = now;
      impactCount[i]++;

      // Record the impact angle (resultant of pitch/roll) at this instant
      float impactAngle = sqrt(pitch * pitch + roll * roll);
      impactAngleHistory[impactHistoryIndex] = impactAngle;
      impactHistoryIndex = (impactHistoryIndex + 1) % IMPACT_HISTORY_SIZE;
      if (impactHistoryCount < IMPACT_HISTORY_SIZE) impactHistoryCount++;
    }
  }
}

// =====================================================================
// WEB HANDLERS
// =====================================================================
void handleData() {
  StaticJsonDocument<1024> doc;

  // Live sensor readings
  doc["pitch"] = mpu.getAngleX();
  doc["roll"]  = mpu.getAngleY();

  JsonArray flex = doc.createNestedArray("flex_pct");
  flex.add(flexBendPct[0]);
  flex.add(flexBendPct[1]);
  flex.add(flexBendPct[2]);

  JsonArray flexRawArr = doc.createNestedArray("flex_raw");
  flexRawArr.add(flexRaw[0]);
  flexRawArr.add(flexRaw[1]);
  flexRawArr.add(flexRaw[2]);

  // Average flex bend across the whole session (used for foot diagnosis)
  JsonArray flexAvg = doc.createNestedArray("flex_avg_pct");
  for (int i = 0; i < 3; i++) {
    float avg = flexSampleCount > 0 ? (flexBendPctSum[i] / flexSampleCount) : 0;
    flexAvg.add(avg);
  }

  doc["step_count"] = stepCount;
  doc["piezo1_impacts"] = impactCount[0];
  doc["piezo2_impacts"] = impactCount[1];

  // Average impact angle across recorded history
  float impactAngleSum = 0;
  for (int i = 0; i < impactHistoryCount; i++) impactAngleSum += impactAngleHistory[i];
  float avgImpactAngle = impactHistoryCount > 0 ? (impactAngleSum / impactHistoryCount) : 0;
  doc["avg_impact_angle"] = avgImpactAngle;

  doc["session_seconds"] = (millis() - sessionStartTime) / 1000;

  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleReset() {
  stepCount = 0;
  impactCount[0] = 0;
  impactCount[1] = 0;
  impactHistoryIndex = 0;
  impactHistoryCount = 0;
  flexBendPctSum[0] = flexBendPctSum[1] = flexBendPctSum[2] = 0;
  flexSampleCount = 0;
  sessionStartTime = millis();

  server.send(200, "text/plain", "Session reset.");
}
