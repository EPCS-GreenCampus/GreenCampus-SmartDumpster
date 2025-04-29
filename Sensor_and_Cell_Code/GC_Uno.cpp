/*
GreenCampus SmartDumpster - Arduino Uno Version
- GC_Uno.cpp

This file is part of the GreenCampus SmartDumpster project, designed to work 
with the SIM7000A module and the Arduino Uno board.

GC_Uno.cpp will contain helper functions for the main Arduino sketch 
(Sensor_and_Cell_Code.ino). You will write the functions here
and include this file in the main sketch. 

Declare the functions in the header file (GC_Uno.h).

This file is designed to be modular, so you can easily add or remove functions
as needed. "GC_UNO.h" includes the necessary libraries and defines the constants 
used throughout the project.
*/

// ===================== INCLUDES ========================
#include "GC_Uno.h"

// ======================== GLOBAL VARIABLES ========================
const char entrypoint[] = "harvest.soracom.io";     // Entrypoint for Soracom Harvest, where data will be sent
const int soracomPort = 80;         // Port for Soracom Harvest, default is 80 for HTTP communication. 

const char apn[] = "soracom.io";    // APN for Soracom, used for GPRS reconnection
const char User[] = "sora";         // User for Soracom, used for GPRS reconnection
const char Pass[] = "sora";         // Password for Soracom, used for GPRS reconnection

// Link to the Soracom Harvest Overview page:
// https://developers.soracom.io/en/docs/harvest/


// Variables for the sensor function
// Yeah, I know this is bad practice, but it's easier to do it this way for now
// and I don't want to deal with passing variables around everywhere.
// Time was tight and this was last minute.
long defaultHeight1,duration, distance, defaultHeight2, dist15, dist60, h15, h60;
long x15, x60;

//Specs for the test dumpster
long testDumpLen = 36;
long testDumpWidth = 24;
long testDumpHeight = 36;

//Change these values to real dumpster's specs when the time comes
long dumpsterHeight = testDumpHeight; //height of the dumpster in inches
long dumpsterWidth = testDumpWidth; //Width of the dumpster in inches
long dumpsterlen = testDumpLen; //length of the dumpster in inches

//Distance when the sensors are not reading anything
long defaultD15= dumpsterlen / cos(radians(15));; //User input of default distance for 15-degree angle
long defaultD60 = dumpsterHeight / cos(radians(30)); //User input of default distance for 60-degree angle
long realHeight = 0; //final height we're taking for the trash

//These are offsets of the test casing
long offsetDist60 = 4.5*cos(60*PI/180); //the offset distance the sensor are placed away from the wall
long offsetHeight60 = 3; //the offset height the sensor are placed away from the wall

long offsetDist15 = 4.5;

long trashVolume = 0; //volume of trash
long totalVolume = dumpsterHeight*dumpsterWidth*dumpsterlen; //volume of the dumpster in cubic inches
long fullnessPer = 0; // fullness percentage
// Fix later when optimizing sensor code


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
It handles GPRS connection, client connection, and HTTP POST request.
  
Todo for upcoming semester (2025 Fall):
Current Improvements:
- Replace with actual data from your sensors. There are two ways to do this.
  1. Replace the mock data with actual sensor data in the JSON payload. You can
  do this by calling or calculating the fullness in the function itself.

  2. Pass the sensor data as parameters to the function. So you can call or
  calculate the fullness in the main loop and pass it to this function. 


Future Improvements:
- Reduce payload size by converting the JSON to a binary format. This will 
  reduce the size of the payload and make it easier to send over the network.
  This will be complicated, but can be done. ECE 270, ECE 362, and ECE 404 
  references will be helpful for this. If you took them. If not, you can 
  try to find students who took them and ask them for help.

  Ex. 0101010010|10101010101|010101|0101|01010|10101|01010101|0101010101010101|010101010
  You can divide up a binary string into bit segments and send them over the network.
  Each segment will be hold a specific value. You can use this method to view certain segments
  and decide what each segment means and what it will hold. 

  Using the two Most Significant Bits (MSB) as examples:
  0101010010 could be the fullness of the dumpster. 10101010101 could be the time.
  Since fullness is a percentage (0-100), you can use the first 7 bits to represent the fullness.

  - Optimize/Modifiy the function to your hearts content. Just keep it functional and working.
  You can modify everything except the HTTP POST request part. This part is correct, so don't change it.
*/
void sendDataToSoracom(Stream &SerialMon, long id, long fullness) {
  TinyGsmClient client(modem, 0);

  // Create JSON document
  // Currently just mock (fake) data for testing
  // Replace with actual data from your sensors
  StaticJsonDocument<2048> jsonDoc;
  jsonDoc["id"] = id; // Sensor ID
  jsonDoc["fullness"] = fullness; // Fullness percentage
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

  // Measure JSON size
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
    while (!modem.gprsConnect(apn, User, Pass)) {
        if (modem.isGprsConnected()) { break; }
      SerialMon.println("GPRS reconnect failed. Delaying 10s and retrying...");
      delay(10000);
    }
  }

  // Connect to Soracom Harvest
  SerialMon.println("Connecting to Soracom Harvest...");
  int retries = 0;
  // Keep trying to connect until success or max retries
  // This is to ensure the connection is established before sending data
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



/*
readSensor - Read data from the ultrasonic sensor.

Parameters:
  sensor - The SoftwareSerial object representing the sensor.

This function reads data from the ultrasonic sensor connected to the
SoftwareSerial port. It waits for the sensor to send data and verifies the checksum.
It returns the distance in inches. If the sensor times out or fails to read data, it 
returns -1.

*/
long readSensor(SoftwareSerial &sensor) {
  sensor.listen(); // Switch to this sensor
  delay(100); // Short stabilization delay
  
  unsigned char data[4] = {0};
  unsigned long startTime = millis();
  
  while (millis() - startTime < 300) { // 300ms timeout
    if (sensor.available() > 0) {
      if (sensor.read() == 0xFF) {
        delay(10); // Small delay to ensure data arrives
        if (sensor.available() >= 3) {
          data[1] = sensor.read(); // High byte
          data[2] = sensor.read(); // Low byte
          data[3] = sensor.read(); // Checksum
          
          // Verify checksum (overflow handled by byte casting)
          if (data[3] == (byte)(0xFF + data[1] + data[2])) {
            return ((data[1] << 8) + data[2]) * 0.0393700787; // Convert to inches
          }
        }
      }
    }
  }
  return -1; // Return error if timeout
}


/* 
GetFullPer - Get the fullness percentage of the dumpster.

Parameters:
  Serial - The serial monitor stream for debug output.
  sensor15 - The SoftwareSerial object for the 15-degree sensor.
  sensor60 - The SoftwareSerial object for the 60-degree sensor.

This function reads data from the two ultrasonic sensors (15-degree and 60-degree)
and calculates the fullness percentage of the dumpster. It uses trigonometry to calculate
the height and distance of the trash based on the sensor readings. The fullness percentage
is calculated based on the volume of the trash and the total volume of the dumpster.

It returns the fullness percentage as a long integer.

Todo:
- Fix the function to work with the two sensors and return the fullness percentage.
- Modify the function to send the fullness percentage to Soracom Harvest.
- Figure out why the function disconnects from the network and refuses to connect again.

*/
long GetFullPer(Stream &Serial, SoftwareSerial &sensor15, SoftwareSerial &sensor60) {
  dist15 = readSensor(sensor15) + offsetDist15; //distance reading of 15-degree sesnsor
  delay(50); // Delay between sensor reads
  dist60 = readSensor(sensor60) + offsetDist60; //distance reading of 60-degree sesnsor
  delay(50); // Delay between sensor reads

  h15 = dumpsterHeight - (dist15 * sin(15*PI/180)); //height reading of the 15 degree sensor
  h60 = dumpsterHeight - ((dist60) * sin(60*PI/180) + offsetHeight60); //height reading of the 60 degree sensor
  x15 = dumpsterlen - (dist15 * cos(radians(15))); //x-axis distance reading of the 15 degree sensor from the end of the bin's wall to trash
  x60 = dumpsterlen - (dist60 * cos(radians(60))); //x-axis distance reading of the 60 degree senso from the end of the bin's wall to trash
  
  Serial.print("dist15: ");
  Serial.println(dist15 - offsetDist15);
  Serial.print("dist60: ");
  Serial.println(dist60 - offsetDist60);
  Serial.print("h60: ");
  Serial.println(h60);
  Serial.print("h15: ");
  Serial.println(h15);

  trashVolume = 0;

  //Check 60-degree if reading things
  //if the detected ditance is shorter than 85% of the defualt distance -> sensor is detecting trash
  if(dist60 <= defaultD60*0.85){
    trashVolume = h60 * dumpsterWidth * x60;
    if(dist15 <= defaultD15*0.85){   
      //Seperate into 2 volume
      //Serial.println("True");
      long bottomVol = h60 * dumpsterWidth * x60;
      long topVol = (x60 + x15) * (h15-h60) / 2 * dumpsterWidth;
      trashVolume = topVol+bottomVol;
      }
  }

  Serial.print("Trash Vol: ");
  Serial.println(trashVolume);

  Serial.print("Total Vol: ");
  Serial.println(totalVolume);
  
  fullnessPer = ((float)trashVolume / (float)totalVolume) * 100; //Calculte the fullness percentage
  Serial.print(fullnessPer);
  Serial.println("%");
  delay(1000); // Main loop delay
  return fullnessPer; //Return the fullness percentage
}
