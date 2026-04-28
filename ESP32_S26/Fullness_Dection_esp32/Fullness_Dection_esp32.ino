#include <HardwareSerial.h>

#define PI 3.14159

// Assign UART ports and pins
#define SENSOR15_RX 10  // Sensor 15° RX (sensor TX → ESP32 RX)
#define SENSOR15_TX 11  // Sensor 15° TX (sensor RX → ESP32 TX)
#define SENSOR60_RX 8  // Sensor 60° RX (sensor TX → ESP32 RX)
#define SENSOR60_TX 9  // Sensor 60° TX (sensor RX → ESP32 TX)

// Create hardware serial ports
HardwareSerial sensor15(1);  // UART1
HardwareSerial sensor60(2);  // UART2

// Define constants and variables
long defaultHeight1, duration, distance, defaultHeight2, dist15, dist60, h15, h60, realHeight;
long trashVolume, dumpsterHeight, dumpsterWidth, dumpsterlen, totalVolume, fullnessPer, x15, x60;
long testDumpLen, testDumpWidth, testDumpHeight, defaultD15, defaultD60, offsetDist60, offsetHeight60, offsetDist15;

void setup() {
  Serial.begin(115200); // Serial monitor
  delay(1000);
  Serial.println("ESP32 Dual Sensor Distance Reader Started");

  // Start hardware serials with pins
  sensor15.begin(9600, SERIAL_8N1, SENSOR15_RX, SENSOR15_TX);
  sensor60.begin(9600, SERIAL_8N1, SENSOR60_RX, SENSOR60_TX);

  // Specs for test dumpster (in inches)
  testDumpLen = 36;
  testDumpWidth = 24;
  testDumpHeight = 36;

  // Set dumpster specs (change when using real dumpster)
  dumpsterHeight = testDumpHeight;
  dumpsterWidth = testDumpWidth;
  dumpsterlen = testDumpLen;

  // Default distances
  defaultD15 = dumpsterlen / cos(radians(15));
  defaultD60 = dumpsterHeight / cos(radians(30));
  realHeight = 0;

  // Sensor mounting offsets
  offsetDist60 = 4.5 * cos(60 * PI / 180);
  offsetHeight60 = 3;
  offsetDist15 = 4.5;

  // Volume initialization
  trashVolume = 0;
  totalVolume = dumpsterHeight * dumpsterWidth * dumpsterlen;
  fullnessPer = 0;
}

void loop() {
  // Read both sensors
  dist15 = readSensor(sensor15) + offsetDist15;
  delay(50);
  dist60 = readSensor(sensor60) + offsetDist60;
  delay(50);

  // Compute heights and x-distances
  h15 = dumpsterHeight - (dist15 * sin(15 * PI / 180));
  h60 = dumpsterHeight - ((dist60) * sin(60 * PI / 180) + offsetHeight60);
  x15 = dumpsterlen - (dist15 * cos(radians(15)));
  x60 = dumpsterlen - (dist60 * cos(radians(60)));

  // Debug printouts
  Serial.print("dist15: ");
  Serial.println(dist15 - offsetDist15);
  Serial.print("dist60: ");
  Serial.println(dist60 - offsetDist60);
  Serial.print("h60: ");
  Serial.println(h60);
  Serial.print("h15: ");
  Serial.println(h15);

  trashVolume = 0;

  // Calculate trash volume
  if (dist60 <= defaultD60 * 0.85) {  // Sensor 60 detects trash
    trashVolume = h60 * dumpsterWidth * x60;

    if (dist15 <= defaultD15 * 0.85) {  // Both detect trash
      long bottomVol = h60 * dumpsterWidth * x60;
      long topVol = (x60 + x15) * (h15 - h60) / 2 * dumpsterWidth;
      trashVolume = topVol + bottomVol;
    }
  }

  // Display results
  Serial.print("Trash Volume: ");
  Serial.println(trashVolume);
  Serial.print("Total Volume: ");
  Serial.println(totalVolume);

  fullnessPer = ((float)trashVolume / (float)totalVolume) * 100;
  Serial.print("Fullness: ");
  Serial.print(fullnessPer);
  Serial.println("%");
  delay(1000);
}

// --- Reading ultrasonic sensor data ---
long readSensor(HardwareSerial &sensor) {
  unsigned char data[4] = {0};
  unsigned long startTime = millis();

  while (millis() - startTime < 300) { // 300ms timeout
    if (sensor.available() > 0) {
      if (sensor.read() == 0xFF) {
        delay(10); // Wait for data
        if (sensor.available() >= 3) {
          data[1] = sensor.read(); // High byte
          data[2] = sensor.read(); // Low byte
          data[3] = sensor.read(); // Checksum

          if (data[3] == (byte)(0xFF + data[1] + data[2])) {
            return ((data[1] << 8) + data[2]) * 0.0393700787; // Convert mm → inches
          }
        }
      }
    }
  }
  return -1; // Timeout or bad data
}