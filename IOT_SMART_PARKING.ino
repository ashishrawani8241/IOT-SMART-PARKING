//final code
#define BLYNK_TEMPLATE_ID "TMPL3TzWGS14D"
#define BLYNK_TEMPLATE_NAME "Smart Car Parking"
#define BLYNK_AUTH_TOKEN "oRAPLHAAG6waObAjfMp01MpXE2XhZKEn"

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <MFRC522.h>  // RFID Library
#include <HTTPClient.h>

// Pin Definitions
#define SS_PIN 5      // RFID SS pin for RFID
#define RST_PIN 22    // RFID RST pin
#define SERVO_ENTRY_PIN 26  // Servo motor pin for Entry gate
#define SERVO_EXIT_PIN 27   // Servo motor pin for Exit gate
#define IR_SENSOR_1_PIN 32  // IR Sensor for parking slot 1
#define IR_SENSOR_2_PIN 33  // IR Sensor for parking slot 2
#define IR_SENSOR_3_PIN 34  // IR Sensor for parking slot 3
#define IR_SENSOR_4_PIN 35  // IR Sensor for parking slot 4
#define BUZZER_PIN 14  // Buzzer pin
#define LED_ENTRY_PIN 4  // Entry gate LED pin
#define LED_EXIT_PIN 12   // Exit gate LED pin

// Servo and LCD setup
Servo entryServo;
Servo exitServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Set LCD I2C address (0x27)

// RFID Reader
MFRC522 rfid(SS_PIN, RST_PIN);  // Create MFRC522 instance

// WiFi credentials
char ssid[] = "dipankar";
char pass[] = "dipankar1";

// Blynk auth token
char auth[] = "oRAPLHAAG6waObAjfMp01MpXE2XhZKEn";

// Variables
String car[4] = {"", "", "", ""};  // Simulating four parking spots
String record = "";
String authorizedUIDs[] = {"13A65AA7", "83233025", "51E86109", "83C82925"};  // Example RFID UIDs for authorized cars   

BlynkTimer timer;

// Function to manually clear a specific line on the 16x2 LCD
void clearLCDLine(int line) {
  lcd.setCursor(0, line);   // Move the cursor to the beginning of the line
  lcd.print("                ");  // Print 16 spaces to clear the line (for a 16x2 display)
}

void setup() {
  Serial.begin(115200);

  // Initialize WiFi and Blynk
  Blynk.begin(auth, ssid, pass);
  Serial.println("Connecting to WiFi...");

  // Wait for Blynk connection
  while (Blynk.connect() == false) {
    delay(100);
  }
  Serial.println("Connected to WiFi and Blynk");

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Parking");
  lcd.setCursor(0, 1);
  lcd.print("System");
  Serial.println("LCD Initialized");

  // Show initialization message for 2 seconds
  delay(2000);  // Wait for 2 seconds to let the message appear

  // Immediately proceed to the parking status display (no need to clear)
  
  // Initialize Servos
  entryServo.attach(SERVO_ENTRY_PIN);
  exitServo.attach(SERVO_EXIT_PIN);

  // Initialize RFID
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("RFID Initialized");

  // Set the initial servo position (closed)
  entryServo.write(0);
  exitServo.write(0);

  // Initialize IR Sensors
  pinMode(IR_SENSOR_1_PIN, INPUT);
  pinMode(IR_SENSOR_2_PIN, INPUT);
  pinMode(IR_SENSOR_3_PIN, INPUT);
  pinMode(IR_SENSOR_4_PIN, INPUT);

   // Initialize Buzzer and LEDs
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_ENTRY_PIN, OUTPUT);
  pinMode(LED_EXIT_PIN, OUTPUT);


  // Set up the Blynk timer to update parking status every second
  timer.setInterval(1000L, updateParkingStatus);  // Check parking status every second
}

void loop() {
  Blynk.run();  // Handle Blynk connection
  timer.run();  // Handle Blynk Timer to update the parking status regularly
  handleRFID(); // Handle RFID for entry/exit
}

// Function to handle RFID car entry/exit logic
void handleRFID() {
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uid = getRFIDUid();

    if (isCarParked(uid)) {  // Check if the car is already parked (exit scenario)
      clearLCDLine(0);  
      lcd.setCursor(0, 0);
      lcd.print("Car Exiting");
      openExitGate();
      delay(1000);
      carExit(uid);  // Car exiting
    } else if (isAuthorized(uid)) {  // If the car is authorized, check for space and allow entry
      if (isParkingAvailable()) {   // Check if there's an available spot
        clearLCDLine(0);
        lcd.setCursor(0, 0);
        lcd.print("Access Granted");
        openEntryGate();
        delay(1000);
        carEntry(uid);  // Car entering
      } else {
        // Parking is full, do not open entry gate
        clearLCDLine(0);
        lcd.setCursor(0, 0);
        lcd.print("Parking Full");
        Serial.println("Parking Full - No space available");
        // Entry gate should NOT open when parking is full
        // Buzzer response for parking full
        digitalWrite(BUZZER_PIN, HIGH);  // Turn on the buzzer
        delay(3000);  // Buzz for 1.5 seconds
        digitalWrite(BUZZER_PIN, LOW);  // Turn off the buzzer
      }
    } else {  // Unauthorized car
      clearLCDLine(0);
      lcd.setCursor(0, 0);
      lcd.print("Access Denied");
      delay(2000);
    }
    rfid.PICC_HaltA();  // Halt the RFID card
  }
}

// Function to get the RFID UID
String getRFIDUid() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid.concat(String(rfid.uid.uidByte[i] < 0x10 ? "0" : ""));
    uid.concat(String(rfid.uid.uidByte[i], HEX));
  }
  uid.toUpperCase();  // Ensure the UID is uppercase
  Serial.println("UID: " + uid);
  return uid;
}

// Function to check if the UID is authorized
bool isAuthorized(String uid) {
  for (int i = 0; i < sizeof(authorizedUIDs) / sizeof(authorizedUIDs[0]); i++) {
    if (uid == authorizedUIDs[i]) {
      return true;  // UID is authorized
    }
  }
  return false;  // UID is not authorized
}

// Function to check if a car is already parked
bool isCarParked(String uid) {
  for (int i = 0; i < 4; i++) {  // Check all four parking spots
    if (car[i] == uid) {
      return true;  // Car with this UID is already parked
    }
  }
  return false;
}

// Function to check if parking is available (if any parking spot is empty)
bool isParkingAvailable() {
  for (int i = 0;  i < 4; i++) {
    if (car[i] == "") {
      return true;  // There is an empty parking spot
    }
  }
  return false;  // No empty spots, parking is full
}

// Function for entering a car (after authentication)
void carEntry(String carX) {
  for (int i = 0; i < 4; i++) {  // Limiting to 4 parking slots
    if (car[i] == "") {
      car[i] = carX;
      record += "\n" + carX + " Entered at " + String(millis() / 1000) + " seconds";
      Blynk.virtualWrite(V3, record);  // Displaying car entry record on Blynk

       // Buzzer and LED response for entry
      digitalWrite(BUZZER_PIN, HIGH);  // Turn on the buzzer
      digitalWrite(LED_ENTRY_PIN, HIGH);  // Turn on entry LED
      delay(1000);  // Buzz for 1 second
      digitalWrite(BUZZER_PIN, LOW);  // Turn off the buzzer
      digitalWrite(LED_ENTRY_PIN, LOW);  // Turn off entry LED

      // Send entry data to Google Sheets
      sendToGoogleSheets(carX, "Entered");
      closeEntryGate();  // Close the gate after entry
      return;
    }
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Parking Full");
  Serial.println("Parking Full");
}

// Function for exiting a car
void carExit(String carX) {
  for (int i = 0; i < 4; i++) {  // Limiting to 4 parking slots
    if (car[i] == carX) {
      car[i] = "";
      record += "\n" + carX + " Exited at " + String(millis() / 1000) + " seconds";
      Blynk.virtualWrite(V3, record);  // Displaying car exit record on Blynk

      // Buzzer and LED response for exit
      digitalWrite(BUZZER_PIN, HIGH);  // Turn on the buzzer
      digitalWrite(LED_EXIT_PIN, HIGH);  // Turn on exit LED
      delay(1000);  // Buzz for 1 second
      digitalWrite(BUZZER_PIN, LOW);  // Turn off the buzzer
      digitalWrite(LED_EXIT_PIN, LOW);  // Turn off exit LED

       // Send exit data to Google Sheets
      sendToGoogleSheets(carX, "Exited");
      closeExitGate();  // Close the exit gate after the car leaves
      return;
    }
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Car not found");
  Serial.println("Car not found.");
}

// Functions to handle servos for entry/exit gates
void openEntryGate() {
  entryServo.write(45);  // Open the entry gate
  delay(3000);  // Hold open for 3 seconds
}

void closeEntryGate() {
  entryServo.write(0);  // Close the entry gate
}

void openExitGate() {
  exitServo.write(45);  // Open the exit gate
  delay(3000);  // Hold open for 3 seconds
}

void closeExitGate() {
  exitServo.write(0);  // Close the exit gate
}

// Function to update parking status based on IR sensor readings
void updateParkingStatus() {
  int sensor1Status = digitalRead(IR_SENSOR_1_PIN);  // Read IR sensor 1
  int sensor2Status = digitalRead(IR_SENSOR_2_PIN);  // Read IR sensor 2
  int sensor3Status = digitalRead(IR_SENSOR_3_PIN);  // Read IR sensor 3
  int sensor4Status = digitalRead(IR_SENSOR_4_PIN);  // Read IR sensor 4

  // Display sensor statuses on Blynk
  Blynk.virtualWrite(V5, sensor1Status == LOW ? "Occupied" : "Empty");
  Blynk.virtualWrite(V6, sensor2Status == LOW ? "Occupied" : "Empty");
  Blynk.virtualWrite(V7, sensor3Status == LOW ? "Occupied" : "Empty");
  Blynk.virtualWrite(V8, sensor4Status == LOW ? "Occupied" : "Empty");

  // Send the number of full slots to the gauge
  int fullSlots = (sensor1Status == LOW) + (sensor2Status == LOW) + (sensor3Status == LOW) + (sensor4Status == LOW);
  Blynk.virtualWrite(V10, fullSlots);  

  // Display on LCD in a compact form (2 slots per line)
  lcd.setCursor(0, 0);
  lcd.print(sensor1Status == LOW ? "S1:F " : "S1:E ");  // Slot 1 status
  lcd.print(sensor2Status == LOW ? "S2:F" : "S2:E");    // Slot 2 status

  lcd.setCursor(0, 1);
  lcd.print(sensor3Status == LOW ? "S3:F " : "S3:E ");  // Slot 3 status
  lcd.print(sensor4Status == LOW ? "S4:F" : "S4:E");    // Slot 4 status
}

// Google sheet response

void sendToGoogleSheets(String uid, String action) {
  if (WiFi.status() == WL_CONNECTED) {  // Check Wi-Fi connection status
    HTTPClient http;
    http.begin("https://script.google.com/macros/s/AKfycby4ftAIO6HAA4ofJFx8NSfb2zfkoAZe5FZcbPvd_SimD4P1kcx0uCcsbG6NoZ5pX2q-2A/exec");  // Your Google Apps Script URL
    http.addHeader("Content-Type", "application/json");

    // Create the JSON payload
    String payload = "{\"uid\":\"" + uid + "\", \"action\":\"" + action + "\", \"timestamp\":\"" + String(millis() / 1000) + "\"}";

    // Send HTTP POST request
    int httpResponseCode = http.POST(payload);

    // Check the response code
    if (httpResponseCode > 0) {
      String response = http.getString();  // Get the response
      Serial.println(httpResponseCode);
      Serial.println(response);
    } else {
      Serial.println("Error in sending POST request");
      Serial.println(httpResponseCode);
    }

    http.end();  // End the connection
  } else {
    Serial.println("WiFi Disconnected");
  }
}
