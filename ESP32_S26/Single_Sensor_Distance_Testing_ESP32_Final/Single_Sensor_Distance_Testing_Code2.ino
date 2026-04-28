#include <dummy.h>

// --- ESP32 Compatible Version ---

// Use HardwareSerial instead of SoftwareSerial
#include <HardwareSerial.h>

// Create a hardware serial port instance (UART2)
HardwareSerial mySerial(2);

// Define connections to sensor (adjust these to your wiring)
#define RX_PIN 11  // GPIO16 (sensor TX -> ESP32 RX)
#define TX_PIN 12  // GPIO17 (sensor RX -> ESP32 TX)

// Array to store incoming serial data
unsigned char data_buffer[4] = {0};

// Integer to store distance
int distance = 0;

// Variable to hold checksum
unsigned char CS;

void setup() {
  Serial.begin(115200);

  // Start hardware serial port on UART2 at 9600
  mySerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  Serial.println("ESP32 Sensor Reader Started");
}

void loop() {
  if (mySerial.available() > 0) {
    delay(4);

    // Check for packet header 0xFF
    if (mySerial.read() == 0xFF) {
      data_buffer[0] = 0xFF;

      // Read next 3 bytes
      for (int i = 1; i < 4; i++) {
        if (mySerial.available()) {
          data_buffer[i] = mySerial.read();
        }
      }

      // Compute checksum
      CS = data_buffer[0] + data_buffer[1] + data_buffer[2];

      // Validate checksum
      if (data_buffer[3] == CS) {
        distance = (data_buffer[1] << 8) + data_buffer[2];

        Serial.print("Distance: ");
        Serial.print(distance);
        Serial.println(" mm");
      }
    }
  }
}
