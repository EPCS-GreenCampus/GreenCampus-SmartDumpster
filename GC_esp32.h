#ifndef GC_esp32_H
#define GC_esp32_H

// ======================== LIBRARY DEFINES ========================
#define TINY_GSM_MODEM_SIM7000      // Must be defined BEFORE including TinyGsmClient
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

// ======================== INCLUDES ========================
#include <TinyGsmClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>
// #include <HardwareSerial.h>
#include <TimeLib.h>

// ======================== EXTERNAL OBJECTS ========================
// Declare objects only if they are defined in the main .ino file
extern TinyGsm modem;
extern Stream &SerialMon;

// ======================== FUNCTION DECLARATIONS ========================
void powerOnModem(int RST, int PWR);
void sendDataToSoracom(Stream &SerialMon);
String getISOTimestamp(TinyGsm& modem);

#endif
