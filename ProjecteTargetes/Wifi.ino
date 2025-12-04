#include <WiFi.h>

// Devuelve true si se conecta exitosamente
bool SetupWifi() {
  Serial.print("Conectant a la xarxa WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);

  while (true) {                // reintenta hasta que conecte
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int intents = 0;
    const int maxIntents = 40; // ~20 segundos (40 * 500ms)
    while (WiFi.status() != WL_CONNECTED && intents < maxIntents) {
      delay(500);
      Serial.print(".");
      intents++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.println("WiFi connectat correctament");
      Serial.print("Adreça IP assignada: ");
      Serial.println(WiFi.localIP());
      return true;
    } else {
      Serial.println();
      Serial.println("ERROR: No s'ha pogut connectar al WiFi");
      Serial.print("Últim codi d'estat WiFi: ");
      Serial.println(WiFi.status());
      Serial.println("Reintentant connexió WiFi en 5 segons...");
      WiFi.disconnect(true);
      delay(5000);             // espera antes de reintentar
    }
  }
}
