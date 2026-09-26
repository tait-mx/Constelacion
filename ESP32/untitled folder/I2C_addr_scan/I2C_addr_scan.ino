#include <Wire.h>

void setup() {
  Wire.begin(); // SDA=21, SCL=22 por defecto
  Serial.begin(115200);
  delay(1000);
  Serial.println("Escaneando I2C...");
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("Dispositivo encontrado en 0x");
      Serial.println(addr, HEX);
    }
  }
}
void loop() {}
