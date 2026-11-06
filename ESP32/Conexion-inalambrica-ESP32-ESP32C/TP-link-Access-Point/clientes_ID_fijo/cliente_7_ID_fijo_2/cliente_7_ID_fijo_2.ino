#include <WiFi.h>

// Cliente 7
const char* ssid     = "constelacion";
const char* password = "01Constelacion01";
const char* host     = "192.168.0.150";
const uint16_t port  = 8080;

const int LED_PIN = 8; // Sink: LOW = ON, HIGH = OFF
const int MI_ID = 7;

const unsigned long TCP_TIMEOUT_MS  = 1500;
const unsigned long RECONNECT_DELAY = 1000;
const unsigned long WIFI_TIMEOUT_MS = 8000;

WiFiClient client;

esp_reset_reason_t razonReset;
bool reportarReset = true;  // solo la primera conexión tras un arranque informa el motivo

const char* nombreReset(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "POWERON";
    case ESP_RST_EXT:      return "EXT";
    case ESP_RST_SW:       return "SW";
    case ESP_RST_PANIC:    return "PANIC";
    case ESP_RST_INT_WDT:  return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT:      return "WDT";
    case ESP_RST_DEEPSLEEP:return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    default:               return "OTRO";
  }
}

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) { delay(10); }

  razonReset = esp_reset_reason();
  Serial.printf("Motivo del reset: %s (%d)\n", nombreReset(razonReset), (int)razonReset);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  for (int i = 0; i <= 3; i++) {
    digitalWrite(LED_PIN, LOW);
    delay(100);
    digitalWrite(LED_PIN, HIGH);
    delay(100);
  }

  Serial.printf("\nCliente TCP - ID fijo: %d\n", MI_ID);

  WiFi.mode(WIFI_STA);
  conectarWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
    conectarWiFi();
    return;
  }

  if (!client.connected()) {
    digitalWrite(LED_PIN, HIGH);
    conectarTCP();
    return;
  }

  if (client.available()) {
    String payload = client.readStringUntil('\n');
    payload.trim();

    if (payload.length() > 0) {
      Serial.print("[TCP Recibido]: ");
      Serial.println(payload);

      if (payload.equalsIgnoreCase("ON")) {
        digitalWrite(LED_PIN, LOW);
      } else if (payload.equalsIgnoreCase("OFF")) {
        digitalWrite(LED_PIN, HIGH);
      }
    }
  }
}

void conectarWiFi() {
  Serial.print("[WiFi] Conectando a AP: ");
  Serial.println(ssid);

  WiFi.disconnect();   // sin apagar la radio
  delay(100);
  WiFi.begin(ssid, password);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt < WIFI_TIMEOUT_MS)) {
    delay(200);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Se aplican DESPUÉS de conectar para asegurar que quedan activos
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_13dBm);
    Serial.printf("\n[WiFi] Conectado. IP: %s  RSSI: %d dBm  TxPower: %d (52 = 13 dBm)\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI(), (int)WiFi.getTxPower());
  } else {
    Serial.println("\n[WiFi] Fallo al asociar con el AP. Reintentando...");
  }
}

void conectarTCP() {
  Serial.printf("[TCP] Abriendo socket con %s:%u\n", host, port);

  client.stop();
  client.setTimeout(TCP_TIMEOUT_MS);

  if (client.connect(host, port, TCP_TIMEOUT_MS)) {
    Serial.println("[TCP] *** Socket conectado ***");
    client.setNoDelay(true);

    // El maestro lee el ID con toInt(), que se detiene en el espacio.
    if (reportarReset) {
      client.printf("ID:%d RST:%s(%d) RSSI:%d\n", MI_ID, nombreReset(razonReset), (int)razonReset, WiFi.RSSI());
      reportarReset = false;
    } else {
      client.printf("ID:%d RST:RECONEXION RSSI:%d\n", MI_ID, WiFi.RSSI());
    }
  } else {
    Serial.println("[TCP] Error de enlace con el servidor. Reintentando...");
    delay(RECONNECT_DELAY);
  }
}
