#include <WiFi.h>

const int LED_PIN = 8; // Sink: LOW = ON, HIGH = OFF

void setup() {
  pinMode(LED_PIN, OUTPUT);
  // Pestañeo inicial para confirmar que el chip no está en loop de reinicio
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, LOW);
    delay(100);
    digitalWrite(LED_PIN, HIGH);
    delay(100);
  }

  Serial.begin(115200);
  delay(1500); // Dar tiempo a la consola USB CDC de macOS para estabilizarse

  Serial.println("\n==========================================");
  Serial.println("  ESP32-C3: Test de Estabilidad de Red ");
  Serial.println("==========================================");

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  
  // Reducir la potencia de emisión WiFi para evitar caídas de energía por USB
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  Serial.println("[INFO] Iniciando escaneo de redes cercanas...");
  int n = WiFi.scanNetworks();
  Serial.print("[INFO] Redes encontradas: ");
  Serial.println(n);

  for (int i = 0; i < n; ++i) {
    Serial.print("  - ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" (RSSI: ");
    Serial.print(WiFi.RSSI(i));
    Serial.println(")");
  }
}

void loop() {
  // Pestañeo suave para verificar latido (heartbeat)
  digitalWrite(LED_PIN, LOW);
  delay(50);
  digitalWrite(LED_PIN, HIGH);
  delay(950);
  Serial.println("[SYSTEM] Sistema activo - Esperando ordenes TCP...");
}
