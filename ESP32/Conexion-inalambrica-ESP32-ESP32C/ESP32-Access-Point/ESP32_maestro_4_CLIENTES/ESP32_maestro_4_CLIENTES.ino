/*
  ESP32 (DevKit clásico) - ACCESS POINT + CONTROLADOR DE LED REMOTO (multi-cliente)
  -------------------------------------------------------------------
  Crea su propia red WiFi y manda comandos "ON" y "OFF" a TODOS los
  ESP32-C3 conectados simultáneamente, haciendo que sus LEDs parpadeen
  de forma remota y en sincronía.

  Cómo probarlo:
  1. Sube este sketch al ESP32 clásico.
  2. Sube el sketch cliente a cada ESP32-C3.
  3. Los LEDs de los C3 deberían empezar a parpadear en sincronía,
     incluso con varios conectados a la vez.
*/

#include <WiFi.h>

const char* ssid     = "ESP32_Red";
const char* password = "12345678";
const uint16_t puerto = 8080;

// Cada cuánto tiempo alterna el LED (en milisegundos)
const unsigned long intervaloParpadeo = 500;

// Cantidad máxima de clientes ESP32-C3 que puede atender simultáneamente
const int MAX_CLIENTES = 4;

WiFiServer servidor(puerto);
WiFiClient clientes[MAX_CLIENTES];

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
  Serial.println("Servidor iniciado. Esperando ESP32-C3...");
}

void loop() {
  // 1. Aceptar nuevas conexiones sin bloquear a las ya existentes
  WiFiClient nuevoCliente = servidor.available();
  if (nuevoCliente) {
    bool asignado = false;
    for (int i = 0; i < MAX_CLIENTES; i++) {
      if (!clientes[i] || !clientes[i].connected()) {
        clientes[i] = nuevoCliente;
        asignado = true;
        Serial.print(">>> Cliente conectado en slot ");
        Serial.println(i);
        break;
      }
    }
    if (!asignado) {
      Serial.println(">>> Cliente rechazado: no hay slots libres");
      nuevoCliente.stop();
    }
  }

  // 2. Enviar comando ON/OFF a todos los clientes conectados
  unsigned long ahora = millis();
  if (ahora - ultimoCambio >= intervaloParpadeo) {
    ultimoCambio = ahora;
    estadoLED = !estadoLED;
    const char* comando = estadoLED ? "ON" : "OFF";

    int enviados = 0;
    for (int i = 0; i < MAX_CLIENTES; i++) {
      if (clientes[i] && clientes[i].connected()) {
        clientes[i].println(comando);
        enviados++;
      }
    }

    Serial.print("[Enviado: ");
    Serial.print(comando);
    Serial.print("] a ");
    Serial.print(enviados);
    Serial.println(" cliente(s)");
  }
}
