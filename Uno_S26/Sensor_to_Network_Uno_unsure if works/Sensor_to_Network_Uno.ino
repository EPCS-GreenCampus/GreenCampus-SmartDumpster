/*
  GreenCampus SmartDumpster - Unified Arduino Uno Code
  Reads two ultrasonic sensors and transmits fullness data to Soracom Harvest
*/

// ======================== CONFIGURATION ========================
#define SENSOR_ID 1  // Sensor Device ID

// ======================== LIBRARIES ============================
#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

#include <TinyGsmClient.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>

// ======================== MODEM PINS ============================
#define MODEM_PWRKEY 6
#define MODEM_RST    7
#define MODEM_TX     12   // Arduino TX → SIM7000 RX
#define MODEM_RX     11   // Arduino RX ← SIM7000 TX

#define SerialMon Serial
SoftwareSerial SerialAT(MODEM_RX, MODEM_TX);  // RX, TX for SIM7000

// ======================== GPRS SETTINGS =========================
const char apn[]  = "soracom.io";
const char user[] = "sora";
const char pass[] = "sora";

TinyGsm modem(SerialAT);
TinyGsmClient client(modem);

// ======================== SENSOR DEFINITIONS ===================
#define SENSOR15_RX 10  // Echo pin (15°)
#define SENSOR15_TX 9   // Trig pin (15°)
#define SENSOR60_RX 8
#define SENSOR60_TX 7

SoftwareSerial sensor15(SENSOR15_RX, SENSOR15_TX);
SoftwareSerial sensor60(SENSOR60_RX, SENSOR60_TX);

#define PI 3.14159

// Dumpster geometry constants (example/test values)
long dumpsterHeight = 36;  // in inches
long dumpsterWidth  = 24;
long dumpsterLen    = 36;

// Offsets and calibration
float offsetDist15 = 4.5;
float offsetDist60 = 4.5 * cos(60 * PI / 180);
float offsetHeight60 = 3;

long defaultD15 = dumpsterLen / cos(15 * PI / 180);
long defaultD60 = dumpsterHeight / cos(30 * PI / 180);

float totalVolume = dumpsterHeight * dumpsterWidth * dumpsterLen; // cubic inches

// ======================== HELPER FUNCTIONS ======================
void powerOnModem(int rst, int pwrkey) {
  pinMode(rst, OUTPUT);
  pinMode(pwrkey, OUTPUT);
  digitalWrite(rst, HIGH);
  digitalWrite(pwrkey, HIGH);
  delay(100);
  digitalWrite(pwrkey, LOW);
  delay(1000);
  digitalWrite(pwrkey, HIGH);
  delay(5000);
}

long readSensor(SoftwareSerial &sensor) {
  sensor.listen();
  delay(100);
  unsigned char data[4] = {0};
  unsigned long startTime = millis();

  while (millis() - startTime < 300) {
    if (sensor.available() > 0) {
      if (sensor.read() == 0xFF) {
        delay(10);
        if (sensor.available() >= 3) {
          data[1] = sensor.read();
          data[2] = sensor.read();
          data[3] = sensor.read();
          if (data[3] == (byte)(0xFF + data[1] + data[2])) {
            return ((data[1] << 8) + data[2]) * 0.0393700787; // inches
          }
        }
      }
    }
  }
  return -1;
}

// ======================== SORACOM UPLOAD ========================
void sendDataToSoracom(float fullness, float trashVolume) {
  if (!modem.isGprsConnected()) {
    SerialMon.println("Reconnecting to GPRS...");
    modem.gprsConnect(apn, user, pass);
  }

  if (client.connect("harvest.soracom.io", 80)) {
    SerialMon.println("Connected to Soracom Harvest!");

    StaticJsonDocument<200> doc;
    doc["sensor_id"] = SENSOR_ID;
    doc["fullness_percent"] = fullness;
    doc["trash_volume"] = trashVolume;

    String jsonData;
    serializeJson(doc, jsonData);

    client.println("POST / HTTP/1.1");
    client.println("Host: harvest.soracom.io");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(jsonData.length());
    client.println();
    client.println(jsonData);

    SerialMon.println("Sent data:");
    SerialMon.println(jsonData);

    delay(100);
    while (client.available()) {
      String line = client.readStringUntil('\n');
      SerialMon.println(line);
    }
    client.stop();
  } else {
    SerialMon.println("❌ Connection to Soracom failed.");
  }
}

// ======================== SETUP ================================
void setup() {
  SerialMon.begin(115200);
  delay(10);
  SerialMon.println("==== SmartDumpster Uno + Soracom ====");

  powerOnModem(MODEM_RST, MODEM_PWRKEY);

  SerialAT.begin(9600);
  delay(300);

  SerialMon.println("Initializing modem...");
  while (!modem.restart()) {
    SerialMon.println("Failed to restart modem, retrying...");
    delay(10000);
  }

  SerialMon.println("Modem initialized. Connecting to Soracom...");
  while (!modem.gprsConnect(apn, user, pass)) {
    SerialMon.println("Failed to connect to GPRS, retrying...");
    delay(10000);
  }

  if (modem.isGprsConnected()) {
    SerialMon.println("✅ GPRS Connected!");
  } else {
    SerialMon.println("❌ GPRS connection failed.");
  }

  sensor15.begin(9600);
  sensor60.begin(9600);
}

// ======================== MAIN LOOP ============================
void loop() {
  long dist15 = readSensor(sensor15) + offsetDist15;
  delay(50);
  long dist60 = readSensor(sensor60) + offsetDist60;
  delay(50);

  float h15 = dumpsterHeight - (dist15 * sin(15 * PI / 180));
  float h60 = dumpsterHeight - ((dist60 * sin(60 * PI / 180)) + offsetHeight60);
  float x15 = dumpsterLen - (dist15 * cos(15 * PI / 180));
  float x60 = dumpsterLen - (dist60 * cos(60 * PI / 180));

  float trashVolume = 0;

  if (dist60 <= defaultD60 * 0.85) {
    trashVolume = h60 * dumpsterWidth * x60;
    if (dist15 <= defaultD15 * 0.85) {
      float bottomVol = h60 * dumpsterWidth * x60;
      float topVol = (x60 + x15) * (h15 - h60) / 2 * dumpsterWidth;
      trashVolume = topVol + bottomVol;
    }
  }

  float fullness = (trashVolume / totalVolume) * 100.0;

  SerialMon.print("Fullness: ");
  SerialMon.print(fullness);
  SerialMon.println("%");
  SerialMon.print("Trash Volume: ");
  SerialMon.println(trashVolume);

  sendDataToSoracom(fullness, trashVolume);
  delay(30000); // send data every 30s
}
