/*
  ESP32 (DevKit clásico) - CLIENTE del router + CONTROLADOR DE LED REMOTO (multi-cliente)
  -------------------------------------------------------------------
  Se conecta al router TP-Link (en vez de crear su propia red) y manda
  comandos "ON" y "OFF" a TODOS los ESP32-C3 conectados simultáneamente,
  haciendo que sus LEDs parpadeen de forma remota y en sincronía.

  Cómo probarlo:
  1. Sube este sketch al ESP32 clásico.
  2. Sube el sketch cliente a cada ESP32-C3, apuntando al SSID del router
     y a la IP fija 192.168.0.150.
  3. Los LEDs de los C3 deberían empezar a parpadear en sincronía,
     incluso con varios conectados a la vez.
*/

#include <WiFi.h>

const char* ssid     = "constelacion";
const char* password = "01Constelacion01";
const uint16_t puerto = 8080;

// IP fija reservada para este maestro en el router (192.168.0.150)
IPAddress ipFija(192, 168, 0, 150);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

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

  WiFi.mode(WIFI_STA);
  WiFi.config(ipFija, gateway, subnet);
  WiFi.begin(ssid, password);

  Serial.print("Conectando a ");
  Serial.println(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("=================================");
  Serial.println("Conectado al router exitosamente");
  Serial.print("IP del ESP32 (maestro): ");
  Serial.println(WiFi.localIP());
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
