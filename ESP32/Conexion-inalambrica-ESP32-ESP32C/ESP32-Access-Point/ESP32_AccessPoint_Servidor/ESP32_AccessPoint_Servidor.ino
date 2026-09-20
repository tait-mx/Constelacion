/*
  ESP32 (DevKit clásico) - ACCESS POINT + SERVIDOR
  ---------------------------------------------------
  Este sketch hace que el ESP32 cree su PROPIA red WiFi
  (sin necesidad de router ni internet) y espere conexiones
  de otros dispositivos, como el ESP32-C3 Super Mini.

  Cómo probarlo:
  1. Sube este sketch al ESP32 clásico.
  2. Abre el Monitor Serial a 115200 baud.
  3. Asegúrate de que el Monitor Serial tenga activado
     "Newline" (o "Both NL & CR") en el desplegable de abajo.
  4. Sube el otro sketch (ESP32C3_Estacion_Cliente.ino) al ESP32-C3.
  5. Escribe un mensaje en cualquiera de los dos Monitores Seriales
     y presiona Enter: debería aparecer en el otro dispositivo.
*/

#include <WiFi.h>

// Nombre y contraseña de la red que va a crear el ESP32.
// La contraseña debe tener mínimo 8 caracteres.
const char* ssid     = "ESP32_Red";
const char* password = "12345678";

// Puerto en el que el servidor va a escuchar conexiones.
const uint16_t puerto = 8080;

WiFiServer servidor(puerto);
WiFiClient cliente;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Crea la red WiFi propia (modo Access Point)
  WiFi.softAP(ssid, password);

  Serial.println("=================================");
  Serial.println("Red WiFi creada exitosamente");
  Serial.print("Nombre de la red (SSID): ");
  Serial.println(ssid);
  Serial.print("IP del ESP32 (Access Point): ");
  Serial.println(WiFi.softAPIP()); // normalmente 192.168.4.1
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
      Serial.println(">>> ESP32-C3 conectado! <<<");
    }
  }

  // Si hay un cliente conectado, revisa mensajes entrantes y salientes
  if (cliente && cliente.connected()) {

    // ¿Llegó un mensaje del ESP32-C3?
    if (cliente.available()) {
      String mensaje = cliente.readStringUntil('\n');
      mensaje.trim();
      Serial.print("[ESP32-C3 dice]: ");
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
}
