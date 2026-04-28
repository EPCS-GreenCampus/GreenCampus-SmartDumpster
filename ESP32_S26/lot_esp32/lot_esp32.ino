#define SENSOR_ID       1
#define TINY_GSM_USE_GPRS true          // Enable GPRS
#define TINY_GSM_USE_WIFI false         // Disable WiFi

#include "GC_esp32.h"                     // Include the custom header file for GreenCampus functions
#include <TinyGsmClient.h>              // Include the TinyGSM library for GSM communication

#define MODEM_PWRKEY     4
#define MODEM_RST        5
#define MODEM_TX         17             // Arduino Uno TX → SIM7000 RX, 
#define MODEM_RX         16             // Arduino Uno RX ← SIM7000 TX

#define SerialMon Serial
#define SerialAT Serial1

// #include <HardwareSerial.h>
// HardwareSerial SerialAT(MODEM_RX, MODEM_TX);  // RX, TX

#if !defined(TINY_GSM_RX_BUFFER)
#define TINY_GSM_RX_BUFFER 650
#endif

#define TINY_GSM_DEBUG SerialMon

const char apn[] = "soracom.io";
const char User[] = "sora";
const char Pass[] = "sora";

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif

void setup() {
    SerialMon.begin(115200);        // Set Serial Monitor to 115200 Baud
    delay(10);
    SerialMon.println("==== SIM7000A ESP32 ====");

    powerOnModem(MODEM_RST, MODEM_PWRKEY);
    const long baud = 9600;     // DO NOT EVER DELETE. CODE WANTS CONSTANT LONG, DONT TRY TO OPTIMIZE
    SerialAT.begin(baud, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(300);  // Allow time for the modem to settle

    SerialMon.println("Initialzing Modem...");
    while(!modem.restart()) {
        SerialMon.println("Failed to restart modem, delaying 10s and retrying");
        delay(10000);
    }
    SerialMon.println("Modem initialized successfully.");
    delay(10000);  // Give time to restart

    SerialMon.print("Connecting to "); SerialMon.println(apn);
    while(!modem.gprsConnect(apn, User, Pass)) {
        // DBG("Failed to connect, delaying 10s and retrying");
        SerialMon.print("Failed to connect, delaying 10s and retrying");
        delay(10000);
    }
    if (modem.isGprsConnected()) {
        // DBG("✅ GPRS is connected");
        SerialMon.println("✅ GPRS is connected");

        String ccid = modem.getSimCCID();
        // DBG("CCID:", ccid);
        SerialMon.print("CCID:"); SerialMon.println(ccid);

        String imei = modem.getIMEI();
        // DBG("IMEI:", imei);
        SerialMon.print("IMEI:"); SerialMon.println(imei);

        String imsi = modem.getIMSI();
        // DBG("IMSI:", imsi);
        SerialMon.print("IMSI:"); SerialMon.println(imsi);

        String cop = modem.getOperator();
        // DBG("Operator:", cop);
        SerialMon.print("Operator:"); SerialMon.println(cop);
    } else {
        SerialMon.println("❌ GPRS not connected");
    }
    SerialMon.println("✅ Network connected!");
}

void loop() {
    // Send JSON data to Soracom
    sendDataToSoracom(SerialMon);
    delay(5000);
}
