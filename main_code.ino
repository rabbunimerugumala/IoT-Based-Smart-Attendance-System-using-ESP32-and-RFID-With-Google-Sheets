#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>

// ================= CONFIG =================
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SCRIPT_URL    = "YOUR_GOOGLE_APPS_SCRIPT_WEB_APP_URL"; // Must end in /exec

// ================= PINS ===================
#define RFID_SS_PIN    5
#define RFID_RST_PIN   27

#define LCD_SDA        21
#define LCD_SCL        22

#define BUZZER_PIN     25
#define GREEN_LED_PIN  26
#define RED_LED_PIN    13     

#define IN_BUTTON_PIN  32
#define OUT_BUTTON_PIN 33

// ================= OBJECTS ================
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

String currentMode = "NONE";

// ================= DATABASE ===============
struct Student {
  const char* uid;
  const char* name;
  const char* rollNo;
};

Student students[] = {
  {"B71C3459", "Ravi Kumar",   "21EEE001"},
  {"97D09C59", "Suresh Reddy", "21EEE002"},
  {"07AB3C59", "Anitha",       "21EEE003"},
  {"2A9E94F2", "Priya",        "21EEE004"},
  {"EBA20AD6", "Karthik",      "21EEE005"},
  {"AB9A02D6", "Divya",        "21EEE006"}
};

const int totalStudents = sizeof(students) / sizeof(students[0]);

// =============== HELPERS ==================
String lastUID = "";
unsigned long lastTapMillis = 0;

void lcdShow(String l1, String l2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(l1.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(l2.substring(0, 16));
}

void showModeScreen() {
  if (currentMode == "IN")       lcdShow("IN MODE",  "Tap Your Card");
  else if (currentMode == "OUT") lcdShow("OUT MODE", "Tap Your Card");
  else                           lcdShow("Attendance Sys", "Select IN/OUT");
}

void beep(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(150);
    digitalWrite(BUZZER_PIN, LOW);
    if (i < times - 1) delay(120);
  }
}

void successIndicator() {
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);
}

void errorIndicator() {
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, HIGH);
}

void allIndicatorsOff() {
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
}

String readRFID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

int findStudent(String uid) {
  for (int i = 0; i < totalStudents; i++) {
    if (uid == students[i].uid) return i;
  }
  return -1;
}

// ============ WI-FI WATCHDOG ==============
void checkWiFi() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 5000) return;
  lastCheck = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi lost, reconnecting...");
    WiFi.disconnect();
    WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  }
}

// ============ GOOGLE SHEETS UPLOAD ========
bool sendToGoogleSheets(String uid, String name, String rollNo) {
  if (WiFi.status() != WL_CONNECTED) {
    lcdShow("Wi-Fi Error", "Check Network");
    beep(2);
    errorIndicator();
    delay(2000);
    allIndicatorsOff();
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();                 // no cert check (simple project)

  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(15000);
  http.setReuse(false);

  if (!http.begin(client, GOOGLE_SCRIPT_URL)) {
    Serial.println("http.begin() failed");
    lcdShow("Upload Failed", "Try Again");
    beep(2);
    errorIndicator();
    delay(2000);
    allIndicatorsOff();
    return false;
  }

  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> doc;
  doc["card_id"]      = uid;
  doc["student_name"] = name;
  doc["roll_no"]      = rollNo;
  doc["mode"]         = currentMode;
  doc["status"]       = currentMode + " Marked";

  String payload;
  serializeJson(doc, payload);
  Serial.println("POST payload: " + payload);

  int httpCode = http.POST(payload);
  String response = http.getString();

  Serial.println("HTTP Code: " + String(httpCode));
  Serial.println("Response : " + response);

  http.end();

  // Apps Script usually answers 200; 302 is also a success
  if (httpCode == 200 || httpCode == 302) {
    lcdShow(name, currentMode + " Marked");
    beep(1);
    successIndicator();
    delay(2000);
    allIndicatorsOff();
    return true;
  } else {
    lcdShow("Upload Failed", "Try Again");
    beep(2);
    errorIndicator();
    delay(2000);
    allIndicatorsOff();
    return false;
  }
}

// =============== BUTTONS ==================
void checkButtons() {
  if (digitalRead(IN_BUTTON_PIN) == LOW) {
    delay(250);
    if (digitalRead(IN_BUTTON_PIN) == LOW) {
      currentMode = "IN";
      beep(1);
      lcdShow("IN MODE", "Tap Your Card");
      Serial.println("Mode = IN");
      while (digitalRead(IN_BUTTON_PIN) == LOW) delay(10);
    }
  }

  if (digitalRead(OUT_BUTTON_PIN) == LOW) {
    delay(250);
    if (digitalRead(OUT_BUTTON_PIN) == LOW) {
      currentMode = "OUT";
      beep(1);
      lcdShow("OUT MODE", "Tap Your Card");
      Serial.println("Mode = OUT");
      while (digitalRead(OUT_BUTTON_PIN) == LOW) delay(10);
    }
  }
}

// ================= SETUP ==================
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(IN_BUTTON_PIN, INPUT_PULLUP);
  pinMode(OUT_BUTTON_PIN, INPUT_PULLUP);

  digitalWrite(BUZZER_PIN, LOW);
  allIndicatorsOff();

  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init();
  lcd.backlight();
  lcdShow("Attendance Sys", "Starting...");

  SPI.begin(18, 19, 23, 5);
  rfid.PCD_Init();
  delay(50);

  byte v = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print("MFRC522 version: 0x");
  Serial.println(v, HEX);
  if (v == 0x00 || v == 0xFF) {
    Serial.println("RFID reader NOT detected!");
    lcdShow("RFID Error", "Check Wiring");
    beep(2);
    errorIndicator();
  } else {
    Serial.println("RFID reader OK");
  }

  lcdShow("Attendance Sys", "Connecting...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
    lcdShow("Wi-Fi Connected", "Select IN/OUT");
  } else {
    Serial.println("Wi-Fi FAILED");
    lcdShow("Wi-Fi Error", "Check Network");
    beep(2);
    errorIndicator();
  }

  delay(1500);
  allIndicatorsOff();
  showModeScreen();

  Serial.println("Ready. Press IN or OUT, then tap a card.");
}

// ================= LOOP ===================
void loop() {
  checkWiFi();
  checkButtons();

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial())   return;

  String uid = readRFID();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  // Ignore the same card re-detected within 3 seconds
  if (uid == lastUID && (millis() - lastTapMillis) < 3000) return;
  lastUID = uid;
  lastTapMillis = millis();

  Serial.println("Card UID: " + uid);

  // No mode selected yet
  if (currentMode == "NONE") {
    lcdShow("Select IN/OUT", "Press Button");
    beep(2);
    errorIndicator();
    delay(2000);
    allIndicatorsOff();
    showModeScreen();
    return;
  }

  int idx = findStudent(uid);

  if (idx >= 0) {
    String name   = students[idx].name;
    String rollNo = students[idx].rollNo;

    lcdShow(name, "Checking...");
    sendToGoogleSheets(uid, name, rollNo);
  } else {
    Serial.println("Unknown card");
    lcdShow("Invalid Card", "Try Again");
    beep(2);
    errorIndicator();
    delay(2000);
    allIndicatorsOff();
  }

  showModeScreen();
  delay(300);
}