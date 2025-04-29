/*
GreenCampus SmartDumpster - Arduino Uno Version

This version of the code is intended to work with an Arduino Uno and SIM7000A module.

This code will try to connect the hardware (mentioned above) to the Cellular Network,
specifically Soracom. Once connected, the code with try to send JSON data to Soracom Harvest.

There is a custom header file (GC_Uno.h) and a custom source file (GC_Uno.cpp) for the 
GreenCampus functions. This code is designed to be modular, so you can easily add or 
remove functions as needed. To add new functions, just declare it in the header file 
and define it in the source file. 


Look Here:
The main difference between this code and IoT_GreenCampus_SmartDumpster_Uni.ino one is 
that this code attempts to calculate the fullness percentage of the dumpster using the 
two sensors and send the data to Soracom.

So far, the code isn't functional, but it's a work in progress.

Whenever the GetFullPer function is called, it disconnects from the network and refuses to
connect again. The main guess is that the two sensors interfere with the modem, but it's not
clear yet. 

Todo:
- Fix the GetFullPer function to work with the two sensors and return the fullness percentage.
    * GetFullPer can already calculate the fullness percentage, but it needs to be modified 
    to send the fullness to the sendDataToSoracom function without disconnecting from the
    network.
- Fix the sendDataToSoracom function to accept the fullness percentage and send it to 
    Soracom Harvest.


Required Libraries:
- TinyGSM by Volodymyr Shymanskyy
- ArduinoJson by Benoit Blanchon
- SoftwareSerial (built-in)
- StreamDebugger (optional, for debugging)
- Time by Michael Margolis (for time handling)
- GC_Uno.h (custom header file for GreenCampus functions)
- GC_Uno.cpp (custom source file for GreenCampus functions)

// Link to TinyGSM examples
// https://github.com/vshymanskyy/TinyGSM/tree/master/examples


Notes:
 - This version of the code is intended to be used with an Arduino Uno
 together with a SIM7000A Shield. So any intent to use other boards needs
 changes to be made first. Primarily, changes need to be made to the Pins and Serial.
 Some boards use only SoftwareSerial, some only use HardwareSerial, some use both.
 You need to check what your board uses and make changes as necessary.

 - Not all boards are SIM compatible. That's why we have a SIM7000A Shield.
 Make sure your board is compatible with using SIM Cards and connecting to Networks.
*/

// ======================== SENSOR ID ========================
// Set ID for the Sensor Device, this is Sensor Device 1 (SD 1)
// Currently, we just hard code the ID so we know which one is which,
// but in the future you can use the SIM ID to log which device is which from Soracom.
// You can probably ping Soracom on here and receive the ID and then pack it along with 
// the JSON Data. Just ask for the CCID.
#define SENSOR_ID       1

// ======================== LIBRARY DEFINES ========================
// TinyGSM requires certain defines to be set for the modem and connection type.
// The following defines are for the SIM7000A modem and GPRS connection.
#define TINY_GSM_MODEM_SIM7000              // Redfined for redundancy, better safe than sorry
#define TINY_GSM_USE_GPRS true              // Enable GPRS
#define TINY_GSM_USE_WIFI false             // Disable WiFi

// ======================== INCLUDES ========================
#include "GC_Uno.h"                     // Include the custom header file for GreenCampus functions
#include <TinyGsmClient.h>              // Include the TinyGSM library for GSM communication


// ======================== PIN DEFINITIONS ========================
// Pins will change depending on the board used.
// The following pin definitions are for the Arduino Uno and SIM7000A Shield.
#define MODEM_PWRKEY     6
#define MODEM_RST        7
#define MODEM_TX         11  // Arduino TX → SIM7000 RX
#define MODEM_RX         10  // Arduino RX ← SIM7000 TX

#define SENSOR15_RX 12 //echo pin
#define SENSOR15_TX 13 //trig pin -- rightmost wire - white wire
#define SENSOR60_RX 8
#define SENSOR60_TX 9

// Link to the SIM7000A schematic:
// https://github.com/botletics/SIM7000-LTE-Shield/blob/master/Schematics/SIM7000%20Shield%20Schematic%20v6.png


// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial


#include <SoftwareSerial.h>
SoftwareSerial SerialAT(MODEM_RX, MODEM_TX);  // RX, TX
SoftwareSerial sensor15(SENSOR15_RX, SENSOR15_TX);      // Could be causing issues
SoftwareSerial sensor60(SENSOR60_RX, SENSOR60_TX);      // Could be causing issues

// Buffer size for AT commands
#if !defined(TINY_GSM_RX_BUFFER)
#define TINY_GSM_RX_BUFFER 650
#endif

// See all AT commands, if wanted
// #define DUMP_AT_COMMANDS

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon
// #define TINY_GSM_DEBUG_DEEP

// Define the APN, User, and Pass for the GPRS connection
// These are the default values for Soracom
const char apn[] = "soracom.io";
const char User[] = "sora";
const char Pass[] = "sora";

// Debug Prints
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

    // Initialize pins
    powerOnModem(MODEM_RST, MODEM_PWRKEY);

    // Begin communication with modem
    const long baud = 9600;     // DO NOT EVER DELETE. CODE WANTS CONSTANT LONG, DONT TRY TO OPTIMIZE
    sensor15.begin(baud);
    delay(300);
    sensor60.begin(baud);       // Could be causing issues 
    delay(300);
    SerialAT.begin(baud);       // Could be causing issues
    delay(300);  // Allow time for the modem to settle

    // Initialize modem
    // Keep trying to restart until success
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

    // Network Connection
    // Keep trying to connect until success
    // DBG("Connecting to", apn);
    SerialMon.print("Connecting to "); SerialMon.println(apn);
    while(!modem.gprsConnect(apn, User, Pass)) {
        // DBG("Failed to connect, delaying 10s and retrying");
        SerialMon.print("Failed to connect, delaying 10s and retrying");
        delay(10000);
    }

    // GPRS Status
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

    // Get the fullness percentage from the sensors
    // When commented, the code works.
    // Uncommenting this line causes the code to disconnect from the network
    // and refuse to connect again.
    // long fullPer = GetFullPer(SerialMon, sensor15, sensor60);
    
    // Hardcoded fullness percentage for testing
    long fullPer = 0;
    sendDataToSoracom(SerialMon, SENSOR_ID, fullPer);
    delay(5000);
}
