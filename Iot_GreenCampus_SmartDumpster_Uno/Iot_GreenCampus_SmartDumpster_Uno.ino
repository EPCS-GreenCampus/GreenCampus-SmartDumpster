/*
GreenCampus SmartDumpster- Arduino Uno Version

This code is designed to work with the Arduino Uno and the SIM7000A GSM module.
It connects to the Soracom network and sends JSON data to a specified endpoint.
The code includes functions for initializing the modem, connecting to the network, and sending data.
It also includes a custom header and source file for GreenCampus functions.
The code is structured to be modular, with separate functions for different tasks.
It uses the TinyGSM library for GSM communication and ArduinoJson for JSON handling.

Required Libraries:
- TinyGSM by Volodymyr Shymanskyy
- ArduinoJson by Benoit Blanchon
- SoftwareSerial (built-in)
- StreamDebugger (optional, for debugging)
- Time by Michael Margolis (for time handling)
- GC_Uno.h (custom header file for GreenCampus functions)
- GC_Uno.cpp (custom source file for GreenCampus functions)

Note:
- Deep sleep is not used due to Arduino Uno limitations.
*/

// ===================== INCLUDES ========================
#include "GC_Uno.h"

// =================== PIN DEFINITIONS ===================
#define MODEM_PWRKEY     6
#define MODEM_RST        7
#define MODEM_TX         11  // Arduino TX → SIM7000 RX
#define MODEM_RX         10  // Arduino RX ← SIM7000 TX

// =================== LIBRARY DEFINES ===================
#define TINY_GSM_MODEM_SIM7000  // Redfined for redundancy, better safe than sorry
#define TINY_GSM_USE_GPRS true // Enable GPRS
#define TINY_GSM_USE_WIFI false // Disable WiFi

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

#include <SoftwareSerial.h>
SoftwareSerial SerialAT(MODEM_RX, MODEM_TX);  // RX, TX

#if !defined(TINY_GSM_RX_BUFFER)
#define TINY_GSM_RX_BUFFER 650
#endif

// See all AT commands, if wanted
#define DUMP_AT_COMMANDS

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon
#define TINY_GSM_DEBUG_DEEP

const char apn[] = "soracom.io";
const char User[] = "sora";
const char Pass[] = "sora";

#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif

void setup() {
    // Initialize debug serial
    SerialMon.begin(115200);        // Set Serial Monitor to 115200 Baud
    delay(10);
    // DBG("==== SIM7000A Uno ====");
    SerialMon.println("==== SIM7000A Uno ====");

    // === Initialize pins ===
    powerOnModem(MODEM_RST, MODEM_PWRKEY);

    // === Begin communication with modem ===
    const long baud = 9600;     // DO NOT EVER DELETE. CODE WANTS CONSTANT LONG, DONT TRY TO OPTIMIZE
    SerialAT.begin(baud);
    delay(300);  // Allow time for the modem to settle

    // DBG("Initializing modem...");
    SerialMon.println("Initialzing Modem");
    while(!modem.restart()) {
        // DBG("Failed to restart modem, delaying 10s and retrying");
        SerialMon.println("Failed to restart modem, delaying 10s and retrying");
        delay(10000);
    }
    // DBG("Modem initialized successfully.");
    SerialMon.println("Modem initialized successfully.");
    delay(10000);  // Give time to restart

    // Keep trying to connect until success
    // DBG("Connecting to", apn);
    SerialMon.print("Connecting to "); SerialMon.println(apn);
    while(!modem.gprsConnect(apn, User, Pass)) {
        // DBG("Failed to connect, delaying 10s and retrying");
        SerialMon.print("Failed to connect, delaying 10s and retrying");
        delay(10000);
    }

    // === GPRS Status ===
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

    // DBG("✅ Network connected!");
    SerialMon.println("✅ Network connected!");
}

void loop() {
    // Send JSON data to Soracom
    sendDataToSoracom(SerialMon);
    delay(5000);
}
