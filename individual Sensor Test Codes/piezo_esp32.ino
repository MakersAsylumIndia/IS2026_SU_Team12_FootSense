/*
  =====================================================================
  PIEZO DISC - STANDALONE (ESP32)
  =====================================================================
  Sensor : 50mm Piezoelectric Disc
  Board  : ESP32 DevKit V1 (38-pin)
  Used for: impact / foot-strike detection (heel or forefoot)

  WIRING (damping resistor in parallel - NOT a voltage divider)
  --------------------------------------------------------------------
    Piezo(+) --+-- GPIO4
                |
            [1M ohm resistor] -- GND
    Piezo(-) -- GND

  IMPORTANT: piezo discs generate their own AC voltage when stressed,
  and can spike beyond safe input levels for an analog pin if used
  unprotected. The 1M ohm resistor wired IN PARALLEL across the
  piezo's two leads dampens these spikes to a safe range. Do not omit
  this resistor.

  This example uses a single piezo on GPIO4. For multiple piezos
  (e.g. heel + forefoot), repeat this exact circuit on another
  ADC1-capable pin (e.g. GPIO33) and duplicate the reading code below
  with its own baseline and threshold, since piezo sensors can vary
  sensor-to-sensor in how large a spike they produce for the same tap.

  NO LIBRARY REQUIRED - uses only the built-in analogRead().

  WHAT THIS DOES
  --------------------------------------------------------------------
  - Measures a resting baseline at startup (do not touch the piezo
    during this step).
  - Continuously reads RAW (unsmoothed) ADC values. Unlike the flex
    sensor, piezo readings are intentionally NOT averaged - a piezo
    impact is a brief, sharp voltage spike, and averaging multiple
    samples together would blunt or miss that spike entirely.
  - Flags an impact event when the deviation from baseline exceeds
    IMPACT_THRESHOLD, with a debounce window so one tap isn't counted
    multiple times as the signal rings down.

  TUNING
  --------------------------------------------------------------------
  IMPACT_THRESHOLD has no universal correct value - it depends on your
  specific piezo, how it's mounted, and how much it's damped/glued.
  Run this sketch first, tap at a realistic pressure (not just a hard
  finger tap), and read off the deviation values reported. Set
  IMPACT_THRESHOLD comfortably above the resting jitter and clearly
  below your smallest real tap.

  SERIAL OUTPUT FORMAT (115200 baud)
  --------------------------------------------------------------------
    raw_adc,deviation,event
  =====================================================================
*/

// ---------------------- PIN DEFINITION ----------------------
#define PIEZO_PIN 4

// ---------------------- IMPACT DETECTION SETTINGS ----------------------
// Starting value only - tune this for your specific sensor (see header).
int IMPACT_THRESHOLD = 50;
const unsigned long IMPACT_DEBOUNCE_MS = 150;

// ---------------------- STATE ----------------------
int restingBaseline = 0;
unsigned long lastImpactTime = 0;
unsigned long impactCount = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { delay(10); }

  Serial.println();
  Serial.println(F("====================================="));
  Serial.println(F(" Piezo Disc - Standalone Test"));
  Serial.println(F("====================================="));

  Serial.println(F("Measuring baseline - do NOT touch the piezo..."));
  delay(500);

  long sum = 0;
  const int samples = 50;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(PIEZO_PIN);
    delay(10);
  }
  restingBaseline = sum / samples;

  Serial.print(F("Resting baseline: "));
  Serial.println(restingBaseline);
  Serial.println(F("Now tap the piezo at a realistic pressure to test."));
  Serial.println();
  Serial.println(F("raw_adc,deviation,event"));
}

void loop() {
  int rawADC = analogRead(PIEZO_PIN);
  int deviation = abs(rawADC - restingBaseline);

  String event = "";
  unsigned long now = millis();

  if (deviation > IMPACT_THRESHOLD && (now - lastImpactTime > IMPACT_DEBOUNCE_MS)) {
    lastImpactTime = now;
    impactCount++;
    event = "IMPACT #" + String(impactCount);
  }

  Serial.print(rawADC);
  Serial.print(',');
  Serial.print(deviation);
  Serial.print(',');
  Serial.println(event);

  delay(5); // fast sampling so brief spikes aren't missed
}
