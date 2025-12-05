#include <Arduino.h>
#include "secrets.h"
#include <LiquidCrystal.h>
#include <ArduinoJson.h>

// Definició dels pins del ESP32 connectats a la pantalla LCD
#define LCD_RS 15
#define LCD_E  2
#define LCD_D4 0
#define LCD_D5 4
#define LCD_D6 16
#define LCD_D7 17

LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// Variables globals que venen d'un altre fitxer (aws_iot.ino)
extern String awsLastMessage;     // Últim missatge rebut d'AWS
extern bool awsMessageNew;        // Indica si hi ha un missatge nou

unsigned long lastReadTime = 0;   // Temps de l’última lectura o missatge
const unsigned long timeoutMsg = 5000; // Temps per tornar al missatge per defecte
bool mostrandoDefault = true;     // Controla si s’està mostrant el missatge per defecte

// Mostra un text qualsevol a la pantalla LCD (fins a 2 línies)
void mostrarLCD(const String &msg) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(msg.substring(0, 16));      // Primera línia
  if (msg.length() > 16) {
    lcd.setCursor(0, 1);
    lcd.print(msg.substring(16, 32));   // Segona línia
  }
}

// Mostra dos textos separats per línies, per a esdeveniments
void mostrarEventoLCD(const String &line1, const String &line2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, 16));

  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0, 16));
}

// Mostra el missatge predeterminat quan no hi ha activitat
void mostrarMensajePorDefecto() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Passin targeta");   // Indicació per l’usuari
  lcd.setCursor(0,1);
  lcd.print("espera...");
  mostrandoDefault = true;
}

// Funció callback que rep missatges del broker MQTT d’AWS
void mqttCallback(String &topic, String &payload) {
  Serial.println("AWS MQTT missatge rebut:");
  Serial.println(payload);

  awsLastMessage = payload;      // Guardem el missatge rebut
  awsMessageNew = true;          // Marquem que hi ha un missatge nou
}

// Prototips de funcions definides en altres fitxers
bool SetupWifi();
void SetupRFID();
bool CheckRFID(String &tag);
void SetupAWS(void (*callback)(String &, String &));
void mssgAWS(const String &msg);
void CheckAWS();

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== ESP32 + RC522 + AWS IoT ===");

  lcd.begin(16, 2);
  mostrarMensajePorDefecto();    // Missatge inicial

  SetupWifi();    // Connexió a xarxa WiFi
  SetupRFID();    // Inicialització del lector RFID
  SetupAWS(mqttCallback); // Connexió a AWS IoT amb la callback
}

void loop() {

  // ─── LECTURA RFID ──────────────────────────────────────────────
  String tag;
  if (CheckRFID(tag)) {   // Si s’ha detectat una targeta RFID
    lastReadTime = millis();
    mostrandoDefault = false;

    // Preparem el missatge JSON per enviar a AWS
    String msg = "{\"tag_rfid\":\"" + tag + "\"}";
    Serial.println("Enviant TAG a AWS: " + msg);

    mssgAWS(msg); // Enviem la informació al núvol
  }

  // ─── PROCESSAR MISSATGES D'AWS ─────────────────────────────────
  CheckAWS(); // Comprova si hi ha missatges MQTT pendents

  if (awsMessageNew) { // Si ha arribat un missatge nou
    StaticJsonDocument<256> doc;

    // Intentem interpretar el JSON rebut
    if (deserializeJson(doc, awsLastMessage) == DeserializationError::Ok) {

      String status = doc["status"] | "N/A";
      int id = doc["id_usuari"] | -1;

      // Traduïm l’estat rebut i ho mostrem a la LCD
      if (status == "ACCESO_PERMITIDO") {
        mostrarEventoLCD("Acces permès", "ID: " + String(id));
      }
      else if (status == "ERROR_ASISTENCIA") {
        mostrarEventoLCD("Acces denegat", "ID: " + String(id));
      }
      else {
        mostrarEventoLCD(status, "");
      }

    } else {
      // Si no és JSON, el mostrem tal qual
      mostrarLCD(awsLastMessage);
    }

    awsMessageNew = false;
    lastReadTime = millis();
    mostrandoDefault = false;
  }

  // ─── TORNAR AL MISSATGE PER DEFECTE ────────────────────────────
  if (!mostrandoDefault && millis() - lastReadTime >= timeoutMsg) {
    mostrarMensajePorDefecto();
  }

  delay(150); // Petita pausa per estabilitat
}
