/*
  ESP32-C3 Super Mini - ESTACIÓN + CLIENTE
  ---------------------------------------------------
  Este sketch conecta el ESP32-C3 a la red WiFi creada
  por el otro ESP32 (Access Point) y se comunica con él
  como cliente TCP.

  Cómo probarlo:
  1. Sube primero el sketch ESP32_AccessPoint_Servidor.ino
     al ESP32 clásico y déjalo corriendo.
  2. Sube este sketch al ESP32-C3.
  3. Abre el Monitor Serial de este ESP32-C3 a 115200 baud,
     con "Newline" activado.
  4. Escribe un mensaje y presiona Enter: debería llegar
     al Monitor Serial del ESP32 clásico, y viceversa.
*/

#include <WiFi.h>

// Deben coincidir EXACTAMENTE con los del ESP32 servidor
const char* ssid     = "ESP32_Red";
const char* password = "12345678";

// IP y puerto del ESP32 servidor.
// 192.168.4.1 es la IP por defecto de un ESP32 en modo Access Point.
const char* ipServidor = "192.168.4.1";
const uint16_t puerto = 8080;

WiFiClient cliente;

void setup() {
  Serial.begin(115200);
  delay(1000);

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

  conectarAlServidor();
}

void conectarAlServidor() {
  Serial.print("Conectando al servidor (ESP32)...");
  while (!cliente.connect(ipServidor, puerto)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.println(">>> Conectado al ESP32! <<<");
}

void loop() {
  // Si por alguna razón se pierde la conexión, reintenta
  if (!cliente.connected()) {
    Serial.println("Conexión perdida. Reintentando...");
    conectarAlServidor();
  }

  // ¿Llegó un mensaje del ESP32?
  if (cliente.available()) {
    String mensaje = cliente.readStringUntil('\n');
    mensaje.trim();
    Serial.print("[ESP32 dice]: ");
    Serial.println(mensaje);
  }

  // ¿Escribiste algo en el Monitor Serial para enviar?
  if (Serial.available()) {
    String salida = Serial.readStringUntil('\n');
    salida.trim();
    cliente.println(salida);
    Serial.print("[Tú enviaste]: ");
    Serial.println(salida);
  }
}
