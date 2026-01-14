/*
 * GreenEdge – Arduino Firmware
 *
 * Reads environmental sensors and controls a smart greenhouse door via Bluetooth.
 *
 * Part of the GreenEdge IoT project.
 */

#include <SoftwareSerial.h>
#include <math.h>
#include <DHT.h>

// ---------------------
//   Macro Definitions
// ---------------------

// --- DHT11 ---
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- Temp LED ---
#define Temp_R 9
#define Temp_G 10
#define Temp_B 11

// --- Door LED ---
#define Door_R 12
#define Door_B 13

// --- Analog Inputs ---
#define LDR_Lum A1
#define LM35_Temp A2
#define Soil_Hum A3

// --- Bluetooth ---
SoftwareSerial ble(2, 3);

// -------------------
//   Global Variables
// -------------------

// --- Constants ---
const float ADC_REF_V = 5.0;
const float ADC_COUNTS = 1023.0;
const float LM35_V_PER_C = 0.01; // 10mV per °C
const float TEMP_THRESHOLD_C = 25.0;

// --- Sensor Variables ---
float dhtTemp = 0.0;
float dhtHum = 0.0;
int lm35 = 0;
float lm35Temp = 0.0;
int soilHum = 0;
int ldrLum = 0;

// --- Door Control ---
bool doorUnlocked = false;
uint32_t doorUnlockedAt = 0;
const uint32_t UNLOCK_DURATION = 3000; // 3 seconds

// -----------------
//   DHT11 Limiter  
// -----------------

uint32_t lastDHTRead = 0;
const uint32_t DHT_INTERVAL = 2000;

// Read DHT11 with interval
void readDHT() {
  if (millis() - lastDHTRead >= DHT_INTERVAL) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t)) dhtTemp = t;
    if (!isnan(h)) dhtHum = h;

    lastDHTRead = millis();
  }
}

// ------------------
//   Main Functions
// ------------------

void setup() {
  // Output Configuration
  pinMode(Temp_R, OUTPUT);
  pinMode(Temp_G, OUTPUT);
  pinMode(Temp_B, OUTPUT);
  pinMode(Door_R, OUTPUT);
  pinMode(Door_B, OUTPUT);
  // Input Configuration
  pinMode(LDR_Lum, INPUT);
  pinMode(LM35_Temp, INPUT);
  pinMode(Soil_Hum, INPUT);

  // Initialize Bluetooth
  ble.begin(9600);

  // Initialize DHT11
  dht.begin();

  // Initial DHT Read
  readDHT();
}

void loop() {
  // Keep DHT values fresh for control logic
  readDHT();

  // Check for Bluetooth commands
  while (ble.available()) {
    char cmd = (char)ble.read();

    // Ignore newline characters
    if (cmd == '\n' || cmd == '\r') continue;

    switch (cmd) {

      // ----- Environmental Sensors -----

      case 'd': // DHT11 Temperature
        ble.print("DHT11_Temp="); ble.println(dhtTemp);
        break;
      case 'h': // DHT11 Humidity
        ble.print("DHT11_Hum="); ble.println(dhtHum);
        break;
      case 't': // LM35 Temperature
        lm35 = analogRead(LM35_Temp);
        lm35Temp = (lm35 * ADC_REF_V / ADC_COUNTS) / LM35_V_PER_C;
        ble.print("LM35_Temp="); ble.println(lm35Temp);
        break;
      case 's': // Soil Humidity
        soilHum = analogRead(Soil_Hum);
        ble.print("Soil_Hum="); ble.println(soilHum);
        break;
      case 'l': // LDR Light Level
        ldrLum = analogRead(LDR_Lum);
        ble.print("LDR_Lum="); ble.println(ldrLum);
        break;

      // ----- Door Control -----

      case 'c': // Door Locked
        doorUnlocked = false;
        ble.println("Door=Locked");
        break;
      case 'o': // Door Unlocked
        doorUnlocked = true;
        doorUnlockedAt = millis();
        ble.println("Door=Unlocked");
        break;
      default:
        // Unknown command
        ble.println("ERR=UnknownCmd");
        break;
    }
  }

  // ----- Auto-Lock Door -----

  if (doorUnlocked && (millis() - doorUnlockedAt >= UNLOCK_DURATION)) {
    doorUnlocked = false;
  }

  // ----- LED Control -----

  if (doorUnlocked) {                // Door Status
    digitalWrite(Door_R, LOW);
    digitalWrite(Door_B, HIGH);
  } else {
    digitalWrite(Door_R, HIGH);
    digitalWrite(Door_B, LOW);
  }

  if (dhtTemp > TEMP_THRESHOLD_C) {  // Temperature Status
    digitalWrite(Temp_R, HIGH);
    digitalWrite(Temp_G, LOW);
    digitalWrite(Temp_B, LOW);
  } else {
    digitalWrite(Temp_R, LOW);
    digitalWrite(Temp_G, HIGH);
    digitalWrite(Temp_B, LOW);
  }
}
