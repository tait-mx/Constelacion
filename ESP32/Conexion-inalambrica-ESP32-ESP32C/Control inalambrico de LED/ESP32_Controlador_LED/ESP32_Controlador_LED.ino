/*
  ESP32 (DevKit clásico) - ACCESS POINT + CONTROLADOR DE LED REMOTO
  -------------------------------------------------------------------
  Este sketch crea su propia red WiFi y manda comandos "ON" y "OFF"
  cada cierto tiempo al ESP32-C3 conectado, haciendo que su LED
  integrado parpadee de forma remota.

  Si este ESP32 se apaga o se desconecta, el ESP32-C3 detecta la
  pérdida de conexión y apaga su LED automáticamente (ver el otro
  sketch: ESP32C3_LED_Remoto.ino).

  Cómo probarlo:
  1. Sube este sketch al ESP32 clásico.
  2. Sube ESP32C3_LED_Remoto.ino al ESP32-C3.
  3. El LED del ESP32-C3 debería empezar a parpadear solo.
  4. Apaga o desconecta el ESP32 (o resetéalo) y el LED del
     ESP32-C3 debería apagarse y quedarse apagado.
  5. Vuelve a conectar el ESP32 y el parpadeo debería reanudarse.
*/

#include <WiFi.h>

const char* ssid     = "ESP32_Red";
const char* password = "12345678";
const uint16_t puerto = 8080;

// Cada cuánto tiempo alterna el LED (en milisegundos)
const unsigned long intervaloParpadeo = 500;

WiFiServer servidor(puerto);
WiFiClient cliente;

bool estadoLED = false;
unsigned long ultimoCambio = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.softAP(ssid, password);

  Serial.println("=================================");
  Serial.println("Red WiFi creada exitosamente");
  Serial.print("Nombre de la red (SSID): ");
  Serial.println(ssid);
  Serial.print("IP del ESP32 (Access Point): ");
  Serial.println(WiFi.softAPIP());
  Serial.println("=================================");

  servidor.begin();
  Serial.println("Servidor iniciado. Esperando al ESP32-C3...");
}

void loop() {
  // Si no hay cliente conectado, revisa si alguien se quiere conectar
  if (!cliente || !cliente.connected()) {
    WiFiClient nuevoCliente = servidor.available();
    if (nuevoCliente) {
      cliente = nuevoCliente;
      Serial.println(">>> ESP32-C3 conectado! Empezando a mandar comandos. <<<");
    }
  }

  // Si hay un cliente conectado, manda el comando ON/OFF cada intervalo
  if (cliente && cliente.connected()) {

    unsigned long ahora = millis();
    if (ahora - ultimoCambio >= intervaloParpadeo) {
      ultimoCambio = ahora;
      estadoLED = !estadoLED;

      if (estadoLED) {
        cliente.println("ON");
        Serial.println("[Enviado]: ON");
      } else {
        cliente.println("OFF");
        Serial.println("[Enviado]: OFF");
      }
    }
  }
}
