#include <LiquidCrystal.h>

// Mantenemos la asignación limpia de pines:
const int PIN_RS = 19;
const int PIN_EN = 23;
const int PIN_D4 = 18;
const int PIN_D5 = 22;
const int PIN_D6 = 4;
const int PIN_D7 = 21;

LiquidCrystal lcd(PIN_RS, PIN_EN, PIN_D4, PIN_D5, PIN_D6, PIN_D7);

void setup() {
  // Configurar pines como salidas para manejo manual
  pinMode(PIN_RS, OUTPUT);
  pinMode(PIN_EN, OUTPUT);
  pinMode(PIN_D4, OUTPUT);
  pinMode(PIN_D5, OUTPUT);
  pinMode(PIN_D6, OUTPUT);
  pinMode(PIN_D7, OUTPUT);

  // --- SECUENCIA DE RESET MANUAL (Segun Datasheet HD44780) ---
  delay(100); // Esperar estabilización de voltaje
  
  // Enviar comando 0x03 tres veces para forzar el modo 8/4 bits
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_RS, LOW);
    digitalWrite(PIN_D4, HIGH);
    digitalWrite(PIN_D5, HIGH);
    digitalWrite(PIN_D6, LOW);
    digitalWrite(PIN_D7, LOW);
    
    // Pulso en ENABLE
    digitalWrite(PIN_EN, HIGH);
    delayMicroseconds(5);
    digitalWrite(PIN_EN, LOW);
    delay(10);
  }

  // Ajustar a modo 4 bits (Comando 0x02)
  digitalWrite(PIN_D4, LOW);
  digitalWrite(PIN_D5, HIGH);
  digitalWrite(PIN_D6, LOW);
  digitalWrite(PIN_D7, LOW);
  digitalWrite(PIN_EN, HIGH);
  delayMicroseconds(5);
  digitalWrite(PIN_EN, LOW);
  delay(10);

  // --- INICIALIZACIÓN CON LA LIBRERÍA ---
  lcd.begin(16, 2);
  lcd.clear();
  lcd.noCursor();
  lcd.noBlink();

  // Escribir mensaje
  lcd.setCursor(0, 0);
  lcd.print("ESP32 Listo!");
  lcd.setCursor(0, 1);
  lcd.print("Pista 01 - STOP");
}

void loop() {
  // Sin código
}
