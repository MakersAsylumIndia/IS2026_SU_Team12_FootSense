/*
  =====================================================================
  MPU6050 - STANDALONE (ESP32)
  =====================================================================
  Sensor : MPU6050 (6-axis gyroscope + accelerometer)
  Board  : ESP32 DevKit V1 (38-pin)
  Used for: step counting (pedometer) and impact angle (pitch/roll)

  WIRING
  --------------------------------------------------------------------
    MPU6050 VCC -> 3V3
    MPU6050 GND -> GND
    MPU6050 SDA -> GPIO21   (+ 10k ohm pull-up resistor to 3V3)
    MPU6050 SCL -> GPIO22   (+ 10k ohm pull-up resistor to 3V3)

  Pull-up resistors are required on most bare MPU6050 breakout boards,
  since they don't include onboard pull-ups themselves.

  LIBRARY REQUIRED
  --------------------------------------------------------------------
    "MPU6050_light" by rfetick (Arduino Library Manager)

  WHAT THIS DOES
  --------------------------------------------------------------------
  - Connects to the MPU6050 and calibrates it at startup (keep the
    sensor flat and still during this step).
  - Continuously reads acceleration (g) and filtered pitch/roll/yaw
    angles (degrees).
  - Runs simple step detection using accelerometer magnitude with
    a high/low threshold and a debounce window, so a single footstep
    isn't counted multiple times.
  - Prints all data to Serial Monitor as CSV.

  SERIAL OUTPUT FORMAT (115200 baud)
  --------------------------------------------------------------------
    accX,accY,accZ,pitch,roll,yaw,step_count
  =====================================================================
*/

#include <Wire.h>
#include <MPU6050_light.h>

// ---------------------- PIN DEFINITIONS ----------------------
#define I2C_SDA 21
#define I2C_SCL 22

MPU6050 mpu(Wire);

// ---------------------- PEDOMETER SETTINGS ----------------------
const float STEP_THRESHOLD_HIGH = 1.35; // g - peak threshold for step detection
const float STEP_THRESHOLD_LOW  = 0.85; // g - reset threshold (hysteresis)
const unsigned long STEP_DEBOUNCE_MS = 300; // minimum time between counted steps

unsigned long stepCount = 0;
bool stepArmed = true;
unsigned long lastStepTime = 0;

// ---------------------- TIMING ----------------------
unsigned long lastSampleTime = 0;
const unsigned long SAMPLE_INTERVAL_MS = 20; // 50 Hz

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { delay(10); }

  Serial.println();
  Serial.println(F("====================================="));
  Serial.println(F(" MPU6050 - Standalone Test"));
  Serial.println(F("====================================="));

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  byte status = mpu.begin();
  Serial.print(F("MPU6050 status: "));
  Serial.println(status);
  while (status != 0) {
    Serial.println(F("MPU6050 connection failed! Check wiring and pull-up resistors."));
    delay(1000);
    status = mpu.begin();
  }

  Serial.println(F("Calibrating - keep sensor flat and still..."));
  delay(1000);
  mpu.calcOffsets(true, true);
  Serial.println(F("Calibration done."));

  Serial.println();
  Serial.println(F("accX,accY,accZ,pitch,roll,yaw,step_count"));

  lastSampleTime = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - lastSampleTime < SAMPLE_INTERVAL_MS) {
    return;
  }
  lastSampleTime = now;

  mpu.update();

  float accX = mpu.getAccX();
  float accY = mpu.getAccY();
  float accZ = mpu.getAccZ();
  float pitch = mpu.getAngleX();
  float roll  = mpu.getAngleY();
  float yaw   = mpu.getAngleZ();

  float accMagnitude = sqrt(accX * accX + accY * accY + accZ * accZ);

  // ---- Step detection (peak detection with hysteresis) ----
  if (stepArmed && accMagnitude > STEP_THRESHOLD_HIGH) {
    if (now - lastStepTime > STEP_DEBOUNCE_MS) {
      stepCount++;
      lastStepTime = now;
      stepArmed = false;
    }
  } else if (!stepArmed && accMagnitude < STEP_THRESHOLD_LOW) {
    stepArmed = true;
  }

  Serial.print(accX, 2);   Serial.print(',');
  Serial.print(accY, 2);   Serial.print(',');
  Serial.print(accZ, 2);   Serial.print(',');
  Serial.print(pitch, 1);  Serial.print(',');
  Serial.print(roll, 1);   Serial.print(',');
  Serial.print(yaw, 1);    Serial.print(',');
  Serial.println(stepCount);
}
