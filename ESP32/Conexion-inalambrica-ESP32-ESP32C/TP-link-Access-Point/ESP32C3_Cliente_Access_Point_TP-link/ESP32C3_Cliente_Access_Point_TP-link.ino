#include <WiFi.h>

// Configuración de Red (Asegúrate de coincidir exactamente con el Maestro)
const char* ssid     = "constelacion"; 
const char* password = "01Constelacion01";      // Cambia por el password real si el Maestro tiene uno
const char* host     = "192.168.0.150";
const uint16_t port  = 8080;

// Configuración de Hardware ESP32-C3 SuperMini
const int LED_PIN = 8; // Sink: LOW = ON, HIGH = OFF

// Parámetros de Tiempo y Timeout (en ms)
const unsigned long TCP_TIMEOUT_MS  = 300; 
const unsigned long RECONNECT_DELAY = 1000;
const unsigned long WIFI_TIMEOUT_MS = 8000;

WiFiClient client;

void setup() {

  Serial.begin(115200);
  delay(300);
  esp_reset_reason_t razon = esp_reset_reason();
  Serial.print("Motivo del reset anterior: ");
  Serial.println(razon);
  
  // Configuración del LED (Lógica Sink: APAGADO al iniciar)
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  for(int i=0; i <= 3; i++) {
  digitalWrite(LED_PIN, LOW);
  delay(100);
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  }

  Serial.begin(115200);
  delay(1000); 

  Serial.println("\n==================================================");
  Serial.println("  ESP32-C3 SuperMini: Cliente TCP de Alta Disponibilidad ");
  Serial.println("==================================================");

  // Configuración de red para alta estabilidad en puerto USB
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_13dBm); // Mantiene estable la línea de energía USB CDC
  
  conectarWiFi();
}

void loop() {
  // 1. Verificación constante de capa de transporte (WiFi)
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH); // Apagar LED sin red
    conectarWiFi();
    return;
  }

  // 2. Verificación y reconexión activa del Socket TCP
  if (!client.connected()) {
    digitalWrite(LED_PIN, HIGH); // Apagar LED sin Socket TCP
    conectarTCP();
    return;
  }

  // 3. Recepción de comandos enviados por el Maestro
  if (client.available()) {
    String payload = client.readStringUntil('\n');
    payload.trim();

    if (payload.length() > 0) {
      Serial.print("[TCP Recibido]: ");
      Serial.println(payload);

      if (payload.equalsIgnoreCase("ON")) {
        digitalWrite(LED_PIN, LOW);   // Encender (Sink)
      } else if (payload.equalsIgnoreCase("OFF")) {
        digitalWrite(LED_PIN, HIGH);  // Apagar (Sink)
      }
    }
  }
}

void conectarWiFi() {
  Serial.print("[WiFi] Conectando a AP: ");
  Serial.println(ssid);

  WiFi.disconnect(true);
  delay(100);
  WiFi.begin(ssid, password);

  unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt < WIFI_TIMEOUT_MS)) {
    delay(200);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] ¡Conectado de forma exitosa!");
    Serial.print("[WiFi] IP del C3: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] Fallo al asociar con el AP. Reintentando...");
  }
}

void conectarTCP() {
  Serial.print("[TCP] Abriendo socket con ");
  Serial.print(host);
  Serial.print(":");
  Serial.println(port);
  
  // Liberar el socket huérfano antes de abrir uno nuevo
  client.stop();

  // Configurar timeout rápido a nivel de socket
  client.setTimeout(TCP_TIMEOUT_MS); 

  if (client.connect(host, port, TCP_TIMEOUT_MS)) {
    Serial.println("[TCP] *** Socket conectado exitosamente ***");
  } else {
    Serial.println("[TCP] Error de enlace con el servidor. Reintentando...");
    delay(RECONNECT_DELAY);
  }
}
