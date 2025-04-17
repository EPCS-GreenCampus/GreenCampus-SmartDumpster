// =================== PIN DEFINITIONS ===================
#define MODEM_PWRKEY     6
#define MODEM_RST        7
#define MODEM_TX         11  // Arduino TX → SIM7000 RX
#define MODEM_RX         10  // Arduino RX ← SIM7000 TX

// =================== LIBRARY DEFINES ===================
#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_USE_GPRS true // Disable GPRS
#define TINY_GSM_USE_WIFI false // Disable WiFi

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

#include <SoftwareSerial.h>
SoftwareSerial SerialAT(MODEM_RX, MODEM_TX);  // RX, TX

#if !defined(TINY_GSM_RX_BUFFER)
#define TINY_GSM_RX_BUFFER 650
#endif

// See all AT commands, if wanted
// #define DUMP_AT_COMMANDS

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon
// #define TINY_GSM_DEBUG_DEEP

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
  // === Set console baud rate ===
  SerialMon.begin(115200);
  delay(10);
  SerialMon.println("==== SIM7000A Diagnostic Tool ====");

  // === Initialize pins ===
  powerOnModem();

  // === Begin communication with modem ===
  const long baud = 9600;     // DO NOT EVER FUCKING DELETE. CODE WANTS CONSTANT LONG, DONT TRY TO OPTIMIZE
  SerialAT.begin(baud);
  delay(300);  // Allow time for the modem to settle

}

// =================== MODEM SETUP ===================
void powerOnModem() {
  // Power cycle sequence for SIM7000
  pinMode(MODEM_RST, OUTPUT);
  digitalWrite(MODEM_RST, LOW);
  delay(100);
  digitalWrite(MODEM_RST, HIGH);

  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, LOW);
  delay(1200);  // Datasheet specifies at least 1.2s low
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(8000);  // Wait for modem to boot

  SerialMon.println("Modem powered on.");
}

void loop() {
  // === Restart Modem (Soft Reset) ===
  // If this fails, just play with the wires until it works
  delay(1500); // Delay to prevent hiccup restarts (Power loss attempts loop again)
  DBG("[1] Initializing modem...");
  if (!modem.restart()) {
    DBG("Failed to restart modem, delaying 10s and retrying");
    delay(10000);
    return;
  }
  DBG("Modem initialized successfully.");
  delay(10000);  // Give time to restart


  // === Get Modem Info ===
  DBG("[2] Getting modem info...");
  String modemInfo = modem.getModemInfo();
  if (modemInfo.length()) {
    SerialMon.print("Modem Info: "); SerialMon.println(modemInfo);
  } else {
    SerialMon.println("❌ Failed to retrieve modem info.");
  }
  delay(10000);  // Give it time

  
  // === SIM Card Status ===
  DBG("[3] Checking SIM...");
  int simStatus = modem.getSimStatus();
  switch (simStatus) {
    case SIM_READY:
      SerialMon.println("✅ SIM status: READY");
      break;
    case SIM_LOCKED:
      SerialMon.println("🔒 SIM status: LOCKED (PIN or PUK required)");
      break;
    case SIM_ERROR:
      SerialMon.println("❌ SIM status: ERROR (Not inserted or unreadable)");
      break;
    default:
      SerialMon.println("❓ SIM status: UNKNOWN");
      break;
  }
  if (simStatus != SIM_READY) {
    SerialMon.println("❌ SIM is not ready. Halting diagnostics.");
    return;
  }
  delay(10000);  // Give it time

  DBG("Connecting to", apn);
  if (!modem.gprsConnect(apn, User, Pass)) {
    DBG("Failed to connect, delaying 10s and retrying");
    delay(10000);
    return;
  }


  // === GPRS Status ===
  DBG("[4] Checking GPRS status...");
  if (modem.isGprsConnected()) {
    SerialMon.println("✅ GPRS is connected");
    String ccid = modem.getSimCCID();
    DBG("CCID:", ccid);

    String imei = modem.getIMEI();
    DBG("IMEI:", imei);

    String imsi = modem.getIMSI();
    DBG("IMSI:", imsi);

    String cop = modem.getOperator();
    DBG("Operator:", cop);
  } else {
    SerialMon.println("❌ GPRS not connected");
  }


  // === Signal Strength ===
  DBG("[5] Checking signal quality...");
  int16_t signal = modem.getSignalQuality();
  SerialMon.print("Signal quality (0-31): "); SerialMon.println(signal);
  if (signal == 99) {
    SerialMon.println("❌ Signal undetectable (check antenna and bands)");
  } else if (signal > 20) {
    SerialMon.println("⚠️ Weak signal");
  } else {
    SerialMon.println("✅ Signal strength acceptable");
  }
  delay(1500);  // Give it time


  // === Operator Info ===
  DBG("[6] Checking operator...");
  String op = modem.getOperator();
  if (op.length()) {
    SerialMon.print("Operator: "); SerialMon.println(op);
  } else {
    SerialMon.println("❌ No operator detected");
  }


  modem.poweroff();
  DBG("Poweroff.");
  DBG("End of Diagnostic");
  while (true) { modem.maintain(); }  // Do nothing forevermore
}
