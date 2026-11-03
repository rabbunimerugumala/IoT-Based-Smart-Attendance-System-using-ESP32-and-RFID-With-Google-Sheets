#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  5
#define RST_PIN 27

MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n=== ESP32 RFID UID Reader Test ===");

  SPI.begin(18, 19, 23, 5);   // SCK, MISO, MOSI, SS
  rfid.PCD_Init();
  delay(100);

  // Verify the reader responds on SPI
  byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print("MFRC522 Version Register: 0x");
  Serial.println(version, HEX);

  if (version == 0x00 || version == 0xFF) {
    Serial.println(">>> READER NOT DETECTED <<<");
    Serial.println("Check: 3.3V power, GND, and SPI wiring.");
    while (true) { delay(1000); }
  }

  Serial.println("Reader OK. Tap a card or tag...\n");
}

void loop() {
  // Check if a new card is present
  if (!rfid.PICC_IsNewCardPresent()) return;
  
  // Try to read the card's serial number
  if (!rfid.PICC_ReadCardSerial())   return;

  // Print the UID in a clean format
  Serial.print("Card UID: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) Serial.print("0");
    Serial.print(rfid.uid.uidByte[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  
  delay(1000); // Short delay to prevent duplicate reads
}
