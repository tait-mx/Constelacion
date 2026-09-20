/*
  ESP32-C3 Super Mini - LED CONTROLADO REMOTAMENTE
  -------------------------------------------------------------------
  Este sketch se conecta a la red WiFi creada por el ESP32 y prende o
  apaga su LED integrado (GPIO8) según los comandos "ON" / "OFF" que
  reciba.

  Si se pierde la conexión con el ESP32 (por ejemplo, porque se apagó),
  el LED se apaga automáticamente y permanece así hasta reconectar.

  Recuerda: el LED de esta placa usa lógica INVERTIDA.
  LOW  = encendido
  HIGH = apagado
*/

#include <WiFi.h>

const char* ssid     = "ESP32_Red";
const char* password = "12345678";
const char* ipServidor = "192.168.4.1";
const uint16_t puerto = 8080;

const int LED_PIN = 8;

WiFiClient cliente;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  apagarLED(); // Empieza apagado hasta recibir un comando

  Serial.print("Conectando a la red: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("¡Conectado a la red del ESP32!");
  Serial.print("IP asignada a este ESP32-C3: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // Si no está conectado al servidor, intenta conectar y mantiene el LED apagado
  if (!cliente.connected()) {
    apagarLED();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Buscando al ESP32 (servidor)...");
      if (cliente.connect(ipServidor, puerto)) {
        Serial.println(">>> Conectado al ESP32! Esperando comandos... <<<");
      } else {
        delay(500); // Espera antes de reintentar, para no saturar
      }
    } else {
      // Se perdió hasta la red WiFi; sigue intentando reconectar
      WiFi.begin(ssid, password);
      delay(500);
    }
    return;
  }

  // Si está conectado, procesa los comandos que lleguen
  if (cliente.available()) {
    String comando = cliente.readStringUntil('\n');
    comando.trim();

    if (comando == "ON") {
      encenderLED();
    } else if (comando == "OFF") {
      apagarLED();
    }
  }
}

void encenderLED() {
  digitalWrite(LED_PIN, LOW); // LOW = encendido (lógica invertida)
}

void apagarLED() {
  digitalWrite(LED_PIN, HIGH); // HIGH = apagado (lógica invertida)
}
