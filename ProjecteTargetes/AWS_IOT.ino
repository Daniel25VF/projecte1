#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <MQTT.h>
#include "secrets.h"
#include <time.h>

#define AWS_IOT_CLIENT_ID "ESP32_AA"  // Identificador del dispositiu a AWS IoT

WiFiClientSecure net;                // Client segur (TLS) per AWS IoT
MQTTClient awsClient(512);           // Client MQTT amb buffer ampliat

String awsLastMessage = "";          // Últim missatge rebut des d’AWS
bool awsMessageNew = false;          // Indica si tenim un missatge nou pendent

// ---------------- CALLBACK DE MISSATGES MQTT ----------------
// Aquesta funció s’executa automàticament quan arriba un missatge MQTT
void messageHandler(String &topic, String &payload) {
  Serial.println("Missatge d'AWS rebut:");
  Serial.println(payload);

  awsLastMessage = payload;          // Guardem el missatge rebut
  awsMessageNew = true;              // Marquem que és un missatge nou
}

// ------------------- SINCRONITZACIÓ NTP ---------------------
// Sincronitza l’hora del ESP32 amb servidors NTP
bool waitForTimeSync(int timeoutSeconds) {
  Serial.println("Sincronitzant hora NTP...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  time_t now;
  int waited = 0;

  // Esperem fins que el temps sigui vàlid
  while ((now = time(nullptr)) < 1600000000 && waited < timeoutSeconds) {
    Serial.print(".");
    delay(1000);
    waited++;
  }
  Serial.println();

  if (now < 1600000000) {
    Serial.println("ERROR: Hora NTP NO sincronitzada.");
    return false;
  }

  Serial.println("Hora NTP correcta");
  return true;
}

// ------------------- RECONNEXIÓ A AWS IoT -------------------
void reconnectAWS() {
  int intents = 0;
  int maxAttempts = 6;    // Nombre màxim d’intents de connexió
  int backoffMs = 3000;   // Temps d’espera entre intents (augmenta exponencialment)

  while (!awsClient.connected() && intents < maxAttempts) {
    Serial.print("Intentant connexió a AWS IoT... ");

    if (awsClient.connect(AWS_IOT_CLIENT_ID)) {
      Serial.println("Connectat correctament a AWS IoT!");

      // Subscripció al tema MQTT on esperem respostes
      if (!awsClient.subscribe("rfid/bbdd")) {
        Serial.println("ERROR en subscriure’s a rfid/bbdd");
      } else {
        Serial.println("Subscrit correctament a rfid/bbdd");
      }

      return;
    } 
    else {
      Serial.println("Connexió fallida, reintentant...");
      delay(backoffMs);
      backoffMs *= 2;   // Backoff exponencial
      intents++;
    }
  }

  if (!awsClient.connected()) {
    Serial.println("ERROR: No s’ha pogut connectar a AWS IoT.");
  }
}

// ---------------- CONFIGURACIÓ PRINCIPAL AWS ----------------
void SetupAWS(void (*callback)(String &, String &)) {
  Serial.println("Configurant AWS IoT...");

  waitForTimeSync(15);    // Cal tenir l’hora sincronitzada per validar certificats TLS

  // Carregar certificats per a la connexió segura
  net.setCACert(AWS_CERT_CA);       
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Iniciar client MQTT
  awsClient.begin(AWS_IOT_ENDPOINT, 8883, net);

  // Assignem la callback per als missatges
  if (callback != nullptr) {
    awsClient.onMessage(callback);
  } else {
    awsClient.onMessage(messageHandler);
  }

  // Intent de connexió
  reconnectAWS();
}

// -------------------- PUBLICACIÓ A AWS ----------------------
void mssgAWS(const String &msg) {
  // Ens assegurem que estem connectats
  if (!awsClient.connected()) {
    reconnectAWS();
  }

  if (!awsClient.connected()) {
    Serial.println("ERROR: No connectat a AWS. No s’envia.");
    return;
  }

  // Publicació al tema MQTT rfid/tags
  if (awsClient.publish("rfid/tags", msg)) {
    Serial.println("Missatge enviat a AWS");
  } else {
    Serial.println("ERROR enviant missatge MQTT");
  }
}

// ------------------- LOOP MQTT (ha d’anar al loop) --------------------
void CheckAWS() {
  // Si es perd la connexió → reconnectem
  if (!awsClient.connected()) {
    reconnectAWS();
  }

  awsClient.loop();   // Processa missatges entrants
}
