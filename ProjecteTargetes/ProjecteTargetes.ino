#include <Arduino.h>
#include "secrets.h"
#include <LiquidCrystal.h>
#include <ArduinoJson.h>

// Pines del ESP32 conectados a la LCD
#define LCD_RS 15
#define LCD_E  2
#define LCD_D4 0
#define LCD_D5 4
#define LCD_D6 16
#define LCD_D7 17

LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// Variables globales de AWS (definidas en aws_iot.ino)
extern String awsLastMessage;
extern bool awsMessageNew;

unsigned long lastReadTime = 0;
const unsigned long timeoutMsg = 5000;
bool mostrandoDefault = true;

void mostrarLCD(const String &msg) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(msg.substring(0, 16));
  if (msg.length() > 16) {
    lcd.setCursor(0, 1);
    lcd.print(msg.substring(16, 32));
  }
}

void mostrarEventoLCD(const String &line1, const String &line2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, 16));

  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0, 16));
}

void mostrarMensajePorDefecto() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Passin targeta");
  lcd.setCursor(0,1);
  lcd.print("espera...");
  mostrandoDefault = true;
}

// Callback MQTT
void mqttCallback(String &topic, String &payload) {
  Serial.println("AWS MQTT mensaje recibido:");
  Serial.println(payload);

  awsLastMessage = payload;
  awsMessageNew = true;
}

// Prototipos (del otro archivo)
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
  mostrarMensajePorDefecto();

  SetupWifi();
  SetupRFID();
  SetupAWS(mqttCallback);
}

void loop() {

  // Leer tarjeta RFID
  String tag;
  if (CheckRFID(tag)) {
    lastReadTime = millis();
    mostrandoDefault = false;

    String msg = "{\"tag_rfid\":\"" + tag + "\"}";
    Serial.println("Enviando TAG a AWS: " + msg);
    mssgAWS(msg);
  }

  // Procesar mensajes MQTT
  CheckAWS();

  // Si llegó un mensaje AWS → mostrarlo
  if (awsMessageNew) {
    StaticJsonDocument<256> doc;

    if (deserializeJson(doc, awsLastMessage) == DeserializationError::Ok) {

      String status = doc["status"] | "N/A";
      int id = doc["id_usuari"] | -1;

      // Traducción
      if (status == "ACCESO_PERMITIDO") {
        mostrarEventoLCD("Acceso permitido", "ID: " + String(id));
      }
      else if (status == "ERROR_ASISTENCIA") {
        mostrarEventoLCD("Acceso denegado", "ID: " + String(id));
      }
      else {
        mostrarEventoLCD(status, "");
      }

    } else {
      mostrarLCD(awsLastMessage);
    }

    awsMessageNew = false;
    lastReadTime = millis();
    mostrandoDefault = false;
  }

  // Volver al mensaje por defecto después de 5 segundos
  if (!mostrandoDefault && millis() - lastReadTime >= timeoutMsg) {
    mostrarMensajePorDefecto();
  }

  delay(150);
}
