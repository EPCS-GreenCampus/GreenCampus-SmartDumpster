/*
GreenCampus SmartDumpster - Arduino Uno Version
- GC_Uno.h

This header file is part of the GreenCampus project for Arduino Uno.
It contains helper functions for the main Arduino sketch (Iot_GreenCampus_SmartDumpster_Uno.ino).
It is designed to work with the SIM7000A module and the Arduino Uno board.

This is where you will declare the functions and variables used in the main sketch.
It includes the necessary libraries and defines the constants used throughout the project.
*/

// GC_Uno.h
#ifndef GC_UNO_H
#define GC_UNO_H

// ======================== LIBRARY DEFINES ========================
#define TINY_GSM_MODEM_SIM7000

// ======================== INCLUDES ========================
#include <TinyGsmClient.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <TimeLib.h>

extern TinyGsm modem;
extern TinyGsmClient client;
extern Stream &SerialMon;

// ======================== FUNCTION DECLARATIONS ========================
// Add your function declarations here
void powerOnModem(int RST, int PWR);
void sendDataToSoracom(Stream &SerialMon, long id, long fullness);
String getISOTimestamp(TinyGsm& modem);
long readSensor(Stream &sensor);
long GetFullPer(Stream &Serial, SoftwareSerial &sensor15, SoftwareSerial &sensor60);

#endif 
// GC_UNO_H
