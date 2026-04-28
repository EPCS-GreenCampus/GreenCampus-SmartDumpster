#include <SoftwareSerial.h>

// Pins for sensors
#define SENSOR15_RX 10 
#define SENSOR15_TX 11 
#define SENSOR60_RX 9
#define SENSOR60_TX 8
#define PI 3.14159

// Setup software serial for sensors
SoftwareSerial sensor15(SENSOR15_RX, SENSOR15_TX);
SoftwareSerial sensor60(SENSOR60_RX, SENSOR60_TX);

// Physical Constants
const int DUMPSTER_ID = 5; 
long dHeight = 36, dWidth = 24, dLen = 24;
long defaultD15 = 25, defaultD60 = 42; 

void setup() {
  Serial.begin(115200); // Communicate with PC
  sensor15.begin(9600);
  sensor60.begin(9600);
}

void loop() {
  // Read distance from both sensors
  long dist15 = readSensor(sensor15) + 3; 
  delay(100); 
  long dist60 = readSensor(sensor60) + 2;
  delay(100);

  // Height and Volume math
  long h60 = dHeight - (dist60 * sin(60 * PI / 180));
  float totalVol = (float)dHeight * dWidth * dLen;
  float trashVol = (dist60 <= defaultD60) ? (h60 * dWidth * dLen) : 0;
  float fullnessPer = (trashVol / totalVol) * 100.0;

  // SEND DATA TO PC IN JSON FORMAT
  Serial.print("{\"id\":");
  Serial.print(DUMPSTER_ID);
  Serial.print(", \"fullness\":");
  Serial.print(fullnessPer);
  Serial.print(", \"temp\":24.5}\n"); // Simulated temp

  delay(10000); // Update every 10 seconds
}

long readSensor(SoftwareSerial &s) {
  s.listen();
  delay(100); 
  unsigned long start = millis();
  while (millis() - start < 300) {
    if (s.available() && s.peek() == 0xFF) {
      if (s.available() >= 4) {
        s.read(); // Header
        uint8_t h = s.read(), l = s.read(), sum = s.read();
        if (sum == ((0xFF + h + l) & 0xFF)) return ((h << 8) | l) * 0.03937;
      }
    } else if (s.available()) { s.read(); }
  }
  return -1;
}