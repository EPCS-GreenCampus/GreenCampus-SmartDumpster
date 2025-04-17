/*
GreenCampus SmartDumpster- Arduino Uno Version
- GC_Uno.cpp

This file is part of the GreenCampus SmartDumpster project, designed to work 
with the SIM7000A module and the Arduino Uno board.

GC_Uno.cpp will contain helper functions for the main Arduino sketch 
(Iot_GreenCampus_SmartDumpster_Uno.ino). You will write the functions here
and include this file in the main sketch. 

Declare the functions in the header file (GC_Uno.h).

This file is designed to be modular, so you can easily add or remove functions
as needed. "GC_UNO.h" includes the necessary libraries and defines the constants 
used throughout the project.
*/

// ===================== INCLUDES ========================
#include "GC_Uno.h"

// =================== GLOBAL VARIABLES ===================
// Entrypoint for Soracom Harvest
const char entrypoint[] = "harvest.soracom.io";
// This is the entrypoint for Soracom Harvest, where the data will be sent

// Port for Soracom Harvest
const int soracomPort = 80;
// This is the port used for HTTP communication with Soracom Harvest

// Link to the Soracom Harvest Overview page:
// https://developers.soracom.io/en/docs/harvest/

const char apn[] = "soracom.io";
const char User[] = "sora";
const char Pass[] = "sora";

// ===================== FUNCTION DEFINITIONS =======================

/*
powerOnModem - Power on the modem using the RST and PWR pins

Parameters:
  RST - Pin number for the RST pin (reset pin)
  PWR - Pin number for the PWR pin (power key pin)

This function initializes the modem by cycling/toggling the RST and PWR pins.
This is to ensure the modem powers on correctly and is ready for communication.

This is set in mind for Arduino Uno and SIM7000A, but can be adapted for other 
boards. So, check whether you need to change hold times when adapting to the
Arduino Nano ESP32 or ESP8266.

Link to the SIM7000A schematic:
https://github.com/botletics/SIM7000-LTE-Shield/blob/master/Schematics/SIM7000%20Shield%20Schematic%20v6.png

*/
void powerOnModem(int RST, int PWR) {
  // Setup RST Pin
  pinMode(RST, OUTPUT);
  // Toggle RST low for 0.1s
  digitalWrite(RST, LOW);
  delay(100);                   // Hold Time for RST Low
  digitalWrite(RST, HIGH);

  // Setup PWR Pin
  pinMode(PWR, OUTPUT);
  // Hold PWR low for 1.2s, then high for 8s
  digitalWrite(PWR, LOW);
  delay(1200);                  // Hold Time for PWR Low
  digitalWrite(PWR, HIGH);
  delay(8000);                  // Hold Time for PWR High
}


/*
getISOTimestamp - Get the current timestamp from the modem in ISO-8601 format.

Parameters:
  modem - The TinyGsm object representing the modem.

This function sends an AT command to the modem to retrieve the current date 
and time. We do this to ensure the time is accurate and in the correct format 
for Soracom Harvest. Soracom Harvest already sets a timestamp on the server side, 
but we can use this to provide a more accurate timestamp in case of network delays 
or other issues. 


CURRENTLY UNUSED, BUT LEFT FOR FUTURE IMPLEMENTATION.
- Returned time string is incorrect
- Honestly, I'd recommend just rewriting this entirely
- Alternatively, you can use the Soracom Harvest timestamp instead of this one.
  BUT, stakeholdrs might want accurate timestamps for their data.


Examples of valid timestamps used by Soracom Harvest:
Date/Time (ISO-8601): 2022-10-05T11:30:45.000Z
Unix Time (seconds): 1633433445
Unix Time (milliseconds): 1633433445000

*/
String getISOTimestamp(TinyGsm& modem) {
  // Send AT command to get time
  modem.sendAT("+CCLK?");

  // Wait for response and store in 'response'
  String response = "";
  if (modem.waitResponse(1000L, response) != 1) {
    Serial.println("Failed to get modem time.");
    return "";
  }

  // Example response: +CCLK: "24/04/13,13:37:20+00"
  int startQuote = response.indexOf('"');
  int endQuote = response.lastIndexOf('"');

  if (startQuote == -1 || endQuote == -1 || endQuote <= startQuote) {
    Serial.println("Malformed time response.");
    return "";
  }

  // Extract the date/time string from the response
  String timeStr = response.substring(startQuote + 1, endQuote); // Ex. 24/04/13,13:37:20+00
  int commaIndex = timeStr.indexOf(',');
  if (commaIndex == -1) {
    Serial.println("Malformed date/time structure.");
    return "";
  }

  String datePart = timeStr.substring(0, commaIndex);    // "24/04/13"
  String timePart = timeStr.substring(commaIndex + 1);   // "13:37:20+00"

  // Extract offset
  int offsetSignIndex = timePart.indexOf('+');
  if (offsetSignIndex == -1) offsetSignIndex = timePart.indexOf('-');

  String offsetStr = "+00"; // default to UTC
  if (offsetSignIndex != -1) {
    offsetStr = timePart.substring(offsetSignIndex);  // "+00", "-05", etc.
    timePart = timePart.substring(0, offsetSignIndex); // trim offset from time
  }

  // Parse time components
  int yy = datePart.substring(0, 2).toInt();
  int mm = datePart.substring(3, 5).toInt();
  int dd = datePart.substring(6, 8).toInt();
  // Normalize year (assuming years >= 70 are 1970s, else 2000s)
  int fullYear = (yy >= 70) ? (1900 + yy) : (2000 + yy);

  int hour = timePart.substring(0, 2).toInt();
  int minute = timePart.substring(3, 5).toInt();
  int second = timePart.substring(6, 8).toInt();

  // Parse offset
  int offsetMinutes = 0;
  if (offsetStr.length() >= 3) {
    int sign = (offsetStr[0] == '-') ? -1 : 1;
    offsetMinutes = sign * offsetStr.substring(1, 3).toInt() * 15;  // <-- 15 minute units
  }

  // Adjust time to UTC
  tmElements_t tm;
  tm.Year = fullYear - 1970;
  tm.Month = mm;
  tm.Day = dd;
  tm.Hour = hour;
  tm.Minute = minute;
  tm.Second = second;

  time_t localTime = makeTime(tm);
  time_t utcTime = localTime - (offsetMinutes * 60);

  // Break down UTC time into components
  breakTime(utcTime, tm);

  // Format to ISO-8601
  char isoTime[30];  // Enough room for full ISO string
  snprintf(isoTime, sizeof(isoTime), "%04d-%02d-%02dT%02d:%02d:%02d.000Z",
           tm.Year + 1970, tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second);
  return String(isoTime);
}


/*
sendDataToSoracom - Send JSON data to Soracom Harvest.

Parameters:
  SerialMon - The serial monitor stream for debug output.

This function connects to the Soracom Harvest endpoint and sends JSON data.
*/
void sendDataToSoracom(Stream &SerialMon) {
  TinyGsmClient client(modem, 0);

  // Create JSON document
  // Currently just mock (fake) data for testing
  // Replace with actual data from your sensors
  StaticJsonDocument<1024> jsonDoc;
  jsonDoc["id"] = random(1,7);
  jsonDoc["temperature"] = random(0, 120);
  jsonDoc["humidity"] = random(20, 60);
  jsonDoc["status"] = "OK";


  // Optional: Add timestamp to JSON
  // Uncomment the following lines to add a timestamp to the JSON payload
/*  
  // Soracom Harvest expects ISO-8601 Date/Time
  // Date/Time (ISO-8601): 2022-10-05T11:30:45.000Z
  String isoTime = getISOTimestamp(modem);
  delay(10000);
  if (isoTime.length()) {
    jsonDoc["time"] = isoTime;
  } else {
    jsonDoc["time"] = "1970-01-01T00:00:00.000Z"; // Fallback or leave empty
  }
*/

  int contentLength = measureJson(jsonDoc);

  // Close previous connection if still open
  SerialMon.println("Preparing client to connect to Soracom Harvest...");
  if (client.connected()) {
    SerialMon.println("Previous client still connected. Closing...");
    client.stop();
    delay(100);
  }

  // Ensure GPRS is still connected
  if (!modem.isGprsConnected()) {
    SerialMon.println("GPRS not connected. Attempting to reconnect...");
    if (!modem.gprsConnect(apn, User, Pass)) {
      SerialMon.println("GPRS reconnect failed. Aborting send.");
      return;
    }
  }

  SerialMon.println("Connecting to Soracom Harvest...");
  int retries = 0;
  while (!client.connect(entrypoint, soracomPort) && retries < 5) {
    SerialMon.println("Failed to connect to Soracom Harvest, retrying in 5 seconds...");
    retries++;
    delay(5000);
  }

  if (!client.connected()) {
    SerialMon.println("Failed to connect after 5 attempts. Giving up.");
    return;
  }

  // HTTP Post request
  // DO NOT MODIFY THIS PART
  // 
  // These lines format the HTTP POST request. They are correct, so don't change them.
  // The only thing you might want to change is the content of the jsonDoc payload above.
  client.println("POST / HTTP/1.1");
  client.print("Host: "); client.println(entrypoint);
  client.println("Content-Type: application/json");
  client.print("Content-Length: "); client.println(contentLength);
  client.println("Connection: close");
  client.println();
  serializeJson(jsonDoc, client);   // Send payload directly
  client.flush();  // ensure it's sent

  // Read server response
  SerialMon.println("Reading server response");
  String response = "";
  while (client.connected() || client.available()) {
      if (client.available()) {
          String line = client.readStringUntil('\n');
          response += line + "\n";
          SerialMon.println(line);
      }
  }
  SerialMon.println("Server Response: " + response);
  if (client.connected()) {
    client.stop();
    delay(100); // Allow socket to fully close
  }
}
