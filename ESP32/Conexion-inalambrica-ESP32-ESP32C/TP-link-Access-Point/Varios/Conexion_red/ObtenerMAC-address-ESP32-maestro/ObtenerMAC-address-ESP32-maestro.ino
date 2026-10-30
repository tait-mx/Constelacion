#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);   // necesario para que macAddress() devuelva el valor correcto
  WiFi.disconnect();     // no intenta conectarse a nada, solo queremos leer la MAC

  Serial.print("MAC del maestro: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  // nada aquí, solo corre una vez
}
