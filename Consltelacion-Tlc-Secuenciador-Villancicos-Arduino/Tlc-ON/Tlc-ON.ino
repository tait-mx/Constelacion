#include "Tlc5940.h"

void setup() {
  Tlc.init();
  for(int i=0; i<=15; i++) {
      Tlc.set(i,4080);  //  rango de 0 a 0
  }
      Tlc.update();
}

void loop() {
  }
  
