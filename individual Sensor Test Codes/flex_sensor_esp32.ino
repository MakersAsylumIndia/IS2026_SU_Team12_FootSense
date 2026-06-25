/*
  =====================================================================
  FLEX SENSOR - STANDALONE (ESP32)
  =====================================================================
  Sensor : 2.2" Flex Sensor (2-pin)
  Board  : ESP32 DevKit V1 (38-pin)
  Used for: arch bend detection (medial / rear / anterior arch zones)

  WIRING (voltage divider - no amplifier needed)
  --------------------------------------------------------------------
    3V3 -- [Flex Sensor] --+-- GPIO34
                             |
                         [33k ohm resistor] -- GND

    Flex sensor lead 1 -> 3V3
    Flex sensor lead 2 -> GPIO34 AND one leg of the 33k resistor
                           (shared junction)
    Resistor other leg -> GND

  This example uses a single flex sensor on GPIO34. For multiple flex
  sensors, repeat this exact divider circuit on other ADC1-capable
  pins (e.g. GPIO35, GPIO32) and duplicate the reading code below.

  NO LIBRARY REQUIRED - uses only the built-in analogRead().

  WHAT THIS DOES
  --------------------------------------------------------------------
  - Measures a resting baseline at startup (keep the sensor flat,
    unbent, during this step).
  - Smooths each reading by averaging several quick back-to-back ADC
    samples, which cancels out a lot of the ESP32 ADC's natural
    sample-to-sample noise.
  - Applies a small deadzone on top of that smoothing: any deviation
    from baseline smaller than FLEX_DEADZONE is reported as exactly 0,
    instead of jittering around small nonzero numbers at rest.
  - Prints raw ADC value and (filtered) deviation from baseline.

  TUNING
  --------------------------------------------------------------------
  - Still seeing small nonzero numbers at rest? Increase FLEX_DEADZONE.
  - Feels unresponsive, takes a firm bend to register? Decrease
    FLEX_DEADZONE, or increase SMOOTHING_SAMPLES instead.

  SERIAL OUTPUT FORMAT (115200 baud)
  --------------------------------------------------------------------
    flex_raw,flex_deviation
  =====================================================================
*/

// ---------------------- PIN DEFINITION ----------------------
#define FLEX_PIN 34

// ---------------------- NOISE FILTERING ----------------------
const int SMOOTHING_SAMPLES = 8;
const int FLEX_DEADZONE = 15; // raw ADC counts

// ---------------------- STATE ----------------------
int flexBaseline = 0;
int flexRaw = 0;
int flexDeviation = 0;

// ---------------------- TIMING ----------------------
unsigned long lastSampleTime = 0;
const unsigned long SAMPLE_INTERVAL_MS = 100; // 10 Hz

// Reads a pin multiple times back-to-back and returns the average,
// which smooths out a lot of the ADC's natural sample-to-sample noise.
int readSmoothed(int pin) {
  long sum = 0;
  for (int i = 0; i < SMOOTHING_SAMPLES; i++) {
    sum += analogRead(pin);
  }
  return sum / SMOOTHING_SAMPLES;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { delay(10); }

  Serial.println();
  Serial.println(F("====================================="));
  Serial.println(F(" Flex Sensor - Standalone Test"));
  Serial.println(F("====================================="));

  analogReadResolution(12); // 12-bit ADC, 0-4095
  pinMode(FLEX_PIN, INPUT);

  Serial.println(F("Calibrating baseline - keep sensor flat, no bend..."));
  delay(500);
  long sum = 0;
  const int calSamples = 50;
  for (int i = 0; i < calSamples; i++) {
    sum += readSmoothed(FLEX_PIN);
    delay(10);
  }
  flexBaseline = sum / calSamples;

  Serial.print(F("Baseline: "));
  Serial.println(flexBaseline);

  Serial.println();
  Serial.println(F("Ready. Bend the sensor to test."));
  Serial.println(F("flex_raw,flex_deviation"));

  lastSampleTime = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - lastSampleTime < SAMPLE_INTERVAL_MS) {
    return;
  }
  lastSampleTime = now;

  flexRaw = readSmoothed(FLEX_PIN);

  int dev = abs(flexRaw - flexBaseline);
  flexDeviation = (dev < FLEX_DEADZONE) ? 0 : dev;

  Serial.print(flexRaw);
  Serial.print(',');
  Serial.println(flexDeviation);
}
