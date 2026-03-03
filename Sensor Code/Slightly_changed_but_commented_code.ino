//This is the current working code with the sensors as of 3-3
#include <SoftwareSerial.h>
#include <AltSoftSerial.h>

// Sensor 1 (60-degree)
AltSoftSerial mySerial1; // RX=8, TX=9 

// Sensor 2 (15-degree)
int pinRX15 = 10;
int pinTX15 = 11;
SoftwareSerial mySerial2(pinRX15, pinTX15);

// Data buffers for each sensor
unsigned char data_buffer1[4] = {0};
unsigned char data_buffer2[4] = {0};
int buffer_index1 = 0;
int buffer_index2 = 0;

// Distance readings in inches
int distance15 = -1;
int distance60 = -1;

// Dumpster configuration
long emptyVolume;
long emptyVolumeSeen;

//THESE ARE ALL THE VALUES YOU MIGHT CHANGE FOR EACH DUMPSTER!!!
//These help calculate the empty volume with the offset
const float EMPTY_READING_60 = 32.0;
const float EMPTY_READING_15 = 19.0;
//These are the dumpster dimensions
long testDumpLen = 22.6;
long testDumpWidth = 24;
long testDumpHeight = 36;
//Calculated based off of the dumpster at 
long topxoffset = 4.25;    
long bottomxoffset = 1.75; 
long topyoffset = 0.6;     
long bottomyoffset = 7.4;  

long angleTop = 15;
long angleBottom = 60;

// Timing for stable readings
unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL = 1000;

// Moving average filter variables
const int FILTER_SIZE = 5;  // Changes how many values are saved
int readings60[FILTER_SIZE];
int readings15[FILTER_SIZE];
int readIndex60 = 0;
int readIndex15 = 0;
long total60 = 0;
long total15 = 0;
int average60 = -1;
int average15 = -1;

// Filter readiness tracking
bool filtersReady = false;
int validReadings60 = 0;
int validReadings15 = 0;

// Spike detection parameters
const int CONSECUTIVE_READINGS = 3;  // This can change how many values is needed to change, right now its 3
int consecutiveCount60 = 0;
int consecutiveCount15 = 0;
int lastRawReading60 = -1;
int lastRawReading15 = -1;
const int SMALL_SPIKE_THRESHOLD = 2;  // Reject spikes smaller than 2 inches
const int CONSISTENCY_TOLERANCE = 1;   // Readings within 1 inch are "consistent"


void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize both serial ports
  mySerial1.begin(9600);
  mySerial2.begin(9600);
  delay(500);
  
  // Initialize filter arrays to zero
  for (int i = 0; i < FILTER_SIZE; i++) {
    readings60[i] = 0;
    readings15[i] = 0;
  }
  
  
  emptyVolume = testDumpHeight * testDumpWidth * testDumpLen;
  emptyVolumeSeen = ((testDumpHeight - topyoffset) * (testDumpLen - topxoffset)) + (.5*(testDumpHeight-topyoffset)*(testDumpHeight-bottomyoffset)*(topxoffset-bottomxoffset));
  
}

void loop() {
  bool sensor1Updated = false;
  bool sensor2Updated = false;
  
  // Read ONLY Sensor 1 for a bit
  unsigned long startTime = millis();
  while (millis() - startTime < 100) {
    while (mySerial1.available() > 0) {
      unsigned char c = mySerial1.read();
      
      if (buffer_index1 == 0 && c != 0xFF) {
        continue;
      }
      
      data_buffer1[buffer_index1++] = c;
      
      if (buffer_index1 >= 4) {
        int newDist = processA02Data(data_buffer1, 1);
        if (newDist >= 0) {
          // Apply calibration factor
          int calibratedDist = (int)(newDist);
          
          //Right now max and min distances are hardcoded when we call the filter
          distance60 = addToFilterSmart(calibratedDist, readings60, readIndex60, 
                                        total60, average60, validReadings60,
                                        consecutiveCount60, lastRawReading60,
                                        .5, 200);
          sensor1Updated = true;
        }
        buffer_index1 = 0;
      }
    }
  }
  
  delay(50);
  
  // Read ONLY Sensor 2 for a bit
  startTime = millis();
  while (millis() - startTime < 100) {
    while (mySerial2.available() > 0) {
      unsigned char c = mySerial2.read();
      
      if (buffer_index2 == 0 && c != 0xFF) {
        continue;
      }
      
      data_buffer2[buffer_index2++] = c;
      
      if (buffer_index2 >= 4) {
        int newDist = processA02Data(data_buffer2, 2);
        if (newDist >= 0) {
          // Apply calibration factor
          int calibratedDist = (int)(newDist);
          
          //Right now max and min distances are hardcoded when we call the filter
          distance15 = addToFilterSmart(calibratedDist, readings15, readIndex15, 
                                        total15, average15, validReadings15,
                                        consecutiveCount15, lastRawReading15,
                                        .5, 200);
          sensor2Updated = true;
        }
        buffer_index2 = 0;
      }
    }
  }
  
  delay(50);
  


if (millis() - lastPrintTime >= PRINT_INTERVAL) {
    lastPrintTime = millis();
    
    Serial.println("=== SENSOR READINGS ===");
    Serial.print("Distance 60° sensor: ");
    if (distance60 >= 0) {
      Serial.print(distance60);
      Serial.print(" inches");
      if (consecutiveCount60 > 0) {
        Serial.print(" (confirming jump: ");
        Serial.print(consecutiveCount60);
        Serial.print("/");
        Serial.print(CONSECUTIVE_READINGS);
        Serial.print(")");
      }
      Serial.println();
    } else {
      Serial.println("NO DATA");
    }
    
    Serial.print("Distance 15° sensor: ");
    if (distance15 >= 0) {
      Serial.print(distance15);
      Serial.print(" inches");
      if (consecutiveCount15 > 0) {
        Serial.print(" (confirming jump: ");
        Serial.print(consecutiveCount15);
        Serial.print("/");
        Serial.print(CONSECUTIVE_READINGS);
        Serial.print(")");
      }
      Serial.println();
    } else {
      Serial.println("NO DATA");
    }
    
    // FIX: Check if BOTH sensors have enough valid readings
    if (validReadings60 < FILTER_SIZE || validReadings15 < FILTER_SIZE) {
      Serial.print("Stabilizing... (60°: ");
      Serial.print(validReadings60);
      Serial.print("/");
      Serial.print(FILTER_SIZE);
      Serial.print(", 15°: ");
      Serial.print(validReadings15);
      Serial.print("/");
      Serial.print(FILTER_SIZE);
      Serial.println(")");
    } else if (distance60 >= 0 && distance15 >= 0) {
      float fullness = calculateVolume(testDumpLen, testDumpWidth, testDumpHeight, 
                                       distance60, distance15, topxoffset, bottomxoffset, topyoffset, bottomyoffset);
      Serial.print("Fullness: ");
      Serial.print(fullness);
      Serial.println("%");
    } else {
      Serial.println("Waiting for valid readings from both sensors...");
    }
    
    Serial.println();
  }
}



// Enhanced filter function with smart jump detection
int addToFilterSmart(int newReading, int* readings, int& readIndex, 
                     long& total, int& average, int& validCount,
                     int& consecutiveCount, int& lastRawReading,
                     int minValid, int maxValid) {
  
  // Reject obvious outliers
  if (newReading < minValid || newReading > maxValid) {
    return average;
  }
  
  // If we don't have a stable average yet, just add it
  if (average < 0) {
    total = total - readings[readIndex];
    readings[readIndex] = newReading;
    total = total + newReading;
    readIndex = (readIndex + 1) % FILTER_SIZE;
    
    if (validCount < FILTER_SIZE) validCount++;
    if (validCount >= FILTER_SIZE) filtersReady = true;
    
    average = total / max(validCount, 1);
    lastRawReading = newReading;
    return average;
  }
  
  // Calculate difference from current average
  int difference = abs(newReading - average);
  
  // CASE 1: Small spike (noise) - reject it
  if (difference <= SMALL_SPIKE_THRESHOLD) {
    // Allow small changes through immediately
    total = total - readings[readIndex];
    readings[readIndex] = newReading;
    total = total + newReading;
    readIndex = (readIndex + 1) % FILTER_SIZE;
    
    if (validCount < FILTER_SIZE) validCount++;
    average = total / FILTER_SIZE;
    
    lastRawReading = newReading;
    consecutiveCount = 0;
    return average;
  }
  
  // CASE 2: Large jump - need consecutive confirmation
  // Check if this reading is consistent with the last raw reading
  bool isConsistent = (lastRawReading > 0 && 
                       abs(newReading - lastRawReading) <= CONSISTENCY_TOLERANCE);
  
  if (isConsistent) {
    consecutiveCount++;
  } else {
    consecutiveCount = 1;  // Reset count, start new sequence
  }
  
  lastRawReading = newReading;
  
  // If we have enough consecutive readings, accept the jump
  if (consecutiveCount >= CONSECUTIVE_READINGS) {
    // Clear the filter and reset with new value
    for (int i = 0; i < FILTER_SIZE; i++) {
      readings[i] = newReading;
    }
    total = newReading * FILTER_SIZE;
    average = newReading;
    validCount = FILTER_SIZE;
    consecutiveCount = 0;
    
    return average;
  }
  
  return average;
}

float calculateVolume(float len, float width, float height,
                      float dist60, float dist15,
                      float topxoffset, float bottomxoffset,
                      float topyoffset, float bottomyoffset) {

  float sin60 = sin(radians(60));
  float cos60 = cos(radians(60));
  float sin15 = sin(radians(15));
  float cos15 = cos(radians(15));

  // Current hit points with implemented offset values
  float h1 = dist60 * sin60 + bottomyoffset;
  float x1 = dist60 * cos60 + bottomxoffset;
  float h2 = dist15 * sin15 + topyoffset;
  float x2 = dist15 * cos15 + topxoffset;

  // Empty baseline hit points
  float h1_empty = EMPTY_READING_60 * sin60 + bottomyoffset;
  float x1_empty = EMPTY_READING_60 * cos60 + bottomxoffset;
  float h2_empty = EMPTY_READING_15 * sin15 + topyoffset;
  float x2_empty = EMPTY_READING_15 * cos15 + topxoffset;

  // Current empty space volume (trapezoid extruded by width)
  // Left side of trapezoid = h1 at position x1 (60° hit)
  // Right side of trapezoid = h2 at position x2 (15° hit)
  float currentEmptyVol = width * (
    (h1 * x1) +                          // rectangle under 60° point
    (0.5 * (h1 + h2) * (x2 - x1)) +     // trapezoid between the two points
    (h2 * (len - x2))                      // final rectangle
  );

  // Empty baseline volume (same formula with empty readings)
  float baselineEmptyVol = width * (
    (h1_empty * x1_empty) +
    (0.5 * (h1_empty + h2_empty) * (x2_empty - x1_empty))
  );

  // Filled volume relative to baseline
  float filledVol = baselineEmptyVol - currentEmptyVol;

  float percentFull = (filledVol / baselineEmptyVol) * 100.0;

  if (percentFull > 100) percentFull = 100;
  if (percentFull < 0) percentFull = 0;

  return percentFull;
}

int processA02Data(unsigned char* buffer, int sensorNum) {
  if (buffer[0] == 0xFF) {
    unsigned char CS = (buffer[0] + buffer[1] + buffer[2]) & 0xFF;
    
    if (CS == buffer[3]) {
      int distance_mm = (buffer[1] << 8) | buffer[2];
      int distance_inches = distance_mm * 0.03937;
      
      return distance_inches;
    } else {
      return -1;
    }
  }
  
  return -1;
}
