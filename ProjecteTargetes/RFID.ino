#include <SPI.h>
#include <MFRC522.h>

// Pines por defecto (ajusta si tu cableado es diferente)
#define SS_PIN 32
#define RST_PIN 21

MFRC522 rfid(SS_PIN, RST_PIN);

void SetupRFID() {
  SPI.begin();            // SCK=18 MOSI=23 MISO=19 por defecto en ESP32
  rfid.PCD_Init();
  Serial.println("RFID inicialitzat correctament (MFRC522)");
  Serial.print("SS_PIN="); Serial.print(SS_PIN);
  Serial.print("  RST_PIN="); Serial.println(RST_PIN);
  Serial.println("Acerca una tarjeta al lector...");
}

// Lee una tarjeta nueva y rellena 'tag' con UID en HEX (si hay)
// Devuelve true si se leyó una tarjeta válida.
bool CheckRFID(String &tag) {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return false;
  }

  tag = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    byte b = rfid.uid.uidByte[i];
    if (b < 0x10) tag += "0";
    tag += String(b, HEX);
  }
  tag.toUpperCase();

  Serial.print("Targeta detectada: ");
  Serial.println(tag);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  return true;
}
