const int sensor1 = A3;// Pad 1 -> A3
const int sensor2 = A1;// Pad 2 -> A1

int rawRest1  = 756;
int rawPress1 = 1023;  

int rawRest2  = 756;  
int rawPress2 = 1023; 

const int numReadings = 8; 

void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.println("Velostat dual pad test starting...");
}

int readAveraged(int pin) {
  long total = 0;
  for (int i = 0; i < numReadings; i++) {
    total += analogRead(pin);
    delay(2);
  }
  return total / numReadings;
}

void loop() {
  int raw1 = readAveraged(sensor1);
  int raw2 = readAveraged(sensor2);

  float volt1 = raw1 * (5.0 / 1023.0);
  float volt2 = raw2 * (5.0 / 1023.0);

  int pressure1 = map(raw1, rawRest1, rawPress1, 0, 100);
  pressure1 = constrain(pressure1, 0, 100);

  int pressure2 = map(raw2, rawRest2, rawPress2, 0, 100);
  pressure2 = constrain(pressure2, 0, 100);

  Serial.print("Pad 1 -> raw: ");
  Serial.print(raw1);
  Serial.print("  voltage: ");
  Serial.print(volt1, 2);
  Serial.print(" V  pressure: ");
  Serial.print(pressure1);
  Serial.println(" %");

  Serial.print("Pad 2 -> raw: ");
  Serial.print(raw2);
  Serial.print("  voltage: ");
  Serial.print(volt2, 2);
  Serial.print(" V  pressure: ");
  Serial.print(pressure2);
  Serial.println(" %");

  Serial.println("------");
  delay(200);
}