#include "Tlc5940.h"

void setup() {
  Tlc.init();
}

void loop() {
  Tlc.set(0, 4080); // Do
  delay(250);
  Tlc.update();
  apagar();
  delay(250);
  
  Tlc.set(2, 4080); // Re
  delay(250);
  Tlc.update();
  apagar();
  delay(250);
  
  Tlc.set(4, 4080); // Mi
  delay(250);
  Tlc.update();
  apagar();
  delay(250);
  
  Tlc.set(5, 4080); // Fa
  delay(250);
  Tlc.update();
  apagar();
  delay(250);
  
  Tlc.set(7, 4080); // Sol
  delay(250);
  Tlc.update();
  apagar();
  delay(250);
  
  Tlc.set(9, 4080); // La
  delay(250);
  Tlc.update();
  apagar();
  delay(250);
  
  Tlc.set(11, 4080); // Si
  delay(250);
  Tlc.update();
  apagar();
  delay(250);
  
  Tlc.set(12, 4080); // Do
  delay(250);
  Tlc.update();
  apagar();
  delay(250);
}

void apagar() {
  for (int i = 0; i <= 15; i++) {
    Tlc.set(i, 0);
    delay(10);
  }
  Tlc.update();
}
