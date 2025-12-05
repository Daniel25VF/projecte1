#include <SPI.h>
#include <MFRC522.h>

// Pins utilitzats per al lector RFID (modifica si el teu cablejat és diferent)
#define SS_PIN 32     // Pin de selecció del lector (SDA/SS)
#define RST_PIN 21    // Pin de reset del lector

// Objecte principal del lector MFRC522
MFRC522 rfid(SS_PIN, RST_PIN);

// ----------------------- CONFIGURACIÓ RFID -----------------------
void SetupRFID() {
  SPI.begin(); 
  rfid.PCD_Init();  // Inicialitza el lector RFID

  Serial.println("RFID inicialitzat correctament (MFRC522)");
  Serial.print("SS_PIN="); Serial.print(SS_PIN);
  Serial.print("  RST_PIN="); Serial.println(RST_PIN);
  Serial.println("Acosta una targeta al lector...");
}

// ----------------------- LECTURA RFID ----------------------------
// Llegeix una targeta nova i guarda el seu UID (HEX) a 'tag'.
// Retorna true si s'ha detectat i llegit una targeta vàlida.
bool CheckRFID(String &tag) {

  // Comprova si hi ha una targeta nova i si es pot llegir el seu número de sèrie
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return false;  // No hi ha targeta nova
  }

  tag = "";

  // Convertim el UID de bytes a cadena HEX
  for (byte i = 0; i < rfid.uid.size; i++) {
    byte b = rfid.uid.uidByte[i];

    if (b < 0x10) tag += "0"; 
    tag += String(b, HEX);     // Converteix el byte a hexadecimal i l'afegeix
  }

  tag.toUpperCase();  // Passem la cadena a majúscules per uniformitat

  Serial.print("Targeta detectada: ");
  Serial.println(tag);

  // Atura la comunicació amb la targeta per evitar interferències
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  return true;  // Targeta llegida correctament
}
