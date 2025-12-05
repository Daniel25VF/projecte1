#include <WiFi.h>

// Retorna true si la connexió a la xarxa WiFi és correcta
bool SetupWifi() {
  Serial.print("Connectant-se a la xarxa WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);  // Mode estàció (client WiFi)

  while (true) {        // Reintenta indefinidament fins connectar
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int intents = 0;
    const int maxIntents = 40;   // Temps màxim d'espera (~20 segons)

    // Esperem a què el WiFi es connecti o s'esgoti el temps d'espera
    while (WiFi.status() != WL_CONNECTED && intents < maxIntents) {
      delay(500);
      Serial.print(".");
      intents++;
    }

    // Si s'ha connectat correctament
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.println("WiFi connectat correctament");
      Serial.print("Adreça IP assignada: ");
      Serial.println(WiFi.localIP());
      return true;
    } 
    
    // Si no s'ha pogut connectar
    else {
      Serial.println();
      Serial.println("ERROR: No s'ha pogut connectar al WiFi");
      Serial.print("Últim codi d'estat WiFi: ");
      Serial.println(WiFi.status());
      Serial.println("Reintentant connexió WiFi en 5 segons...");

      WiFi.disconnect(true);  // Reinicia la interfície WiFi
      delay(5000);            // Temps d’espera abans de reintentar
    }
  }
}
