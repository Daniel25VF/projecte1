#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <MQTT.h>
#include "secrets.h"
#include <time.h>

#define AWS_IOT_CLIENT_ID "ESP32_AA"

WiFiClientSecure net;
MQTTClient awsClient(512);   // buffer aumentado

String awsLastMessage = "";
bool awsMessageNew = false;

// Callback de mensajes MQTT
void messageHandler(String &topic, String &payload) {
  Serial.println("Mensaje AWS recibido:");
  Serial.println(payload);

  awsLastMessage = payload;
  awsMessageNew = true;
}

// -------- NTP SYNC ----------
bool waitForTimeSync(int timeoutSeconds) {
  Serial.println("Sincronizando hora NTP...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  time_t now;
  int waited = 0;

  while ((now = time(nullptr)) < 1600000000 && waited < timeoutSeconds) {
    Serial.print(".");
    delay(1000);
    waited++;
  }
  Serial.println();

  if (now < 1600000000) {
    Serial.println("ERROR: Hora NTP NO sincronizada.");
    return false;
  }

  Serial.println("Hora NTP OK");
  return true;
}

// ---------- MQTT RECONNECT -----------
void reconnectAWS() {
  int intents = 0;
  int maxAttempts = 6;
  int backoffMs = 3000;

  while (!awsClient.connected() && intents < maxAttempts) {
    Serial.print("Intentando conexión a AWS IoT... ");
    if (awsClient.connect(AWS_IOT_CLIENT_ID)) {
      Serial.println("Conectado correctamente a AWS IoT!");

      // 👉 SUSCRIPCIÓN REAL 
      if (!awsClient.subscribe("rfid/bbdd")) {
        Serial.println("ERROR al suscribirse a rfid/bbdd");
      } else {
        Serial.println("Suscrito correctamente a rfid/bbdd");
      }

      return;
    } else {
      Serial.println("Falló la conexión, reintentando...");
      delay(backoffMs);
      backoffMs *= 2;
      intents++;
    }
  }

  if (!awsClient.connected()) {
    Serial.println("ERROR: No se pudo conectar a AWS IoT.");
  }
}


// ---------- CONFIGURACIÓN AWS ----------
void SetupAWS(void (*callback)(String &, String &)) {
  Serial.println("Configurando AWS IoT...");

  waitForTimeSync(15);

  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  awsClient.begin(AWS_IOT_ENDPOINT, 8883, net);

  if (callback != nullptr) {
    awsClient.onMessage(callback);
  } else {
    awsClient.onMessage(messageHandler);
  }

  reconnectAWS();
}

// ---------- PUBLICAR ----------
void mssgAWS(const String &msg) {
  if (!awsClient.connected()) {
    reconnectAWS();
  }

  if (!awsClient.connected()) {
    Serial.println("ERROR: No conectado a AWS. No se envía.");
    return;
  }

  if (awsClient.publish("rfid/tags", msg)) {
    Serial.println("Mensaje enviado a AWS");
  } else {
    Serial.println("ERROR enviando mensaje MQTT");
  }
}

// ---------- LOOP MQTT ----------
void CheckAWS() {
  if (!awsClient.connected()) {
    reconnectAWS();
  }
  awsClient.loop();
}
