#include "Tlc5940.h"

void setup() {
  Tlc.init();
}

void loop() {
      Tlc.set(0,4080);  //  rango de 0 a 0
      Tlc.set(1,4080);
      Tlc.set(2,4080);
      Tlc.set(3,4080);
      Tlc.set(4,4080);
      Tlc.set(5,4080);
      Tlc.set(6,4080);
      Tlc.set(7,4080);
      Tlc.set(8,4080);
      Tlc.set(9,4080);
      Tlc.set(10,4080);
      Tlc.set(11,4080);
      Tlc.set(12,4080);
      Tlc.set(13,4080);
      Tlc.set(14,4080);
      Tlc.set(15,4080);
      Tlc.update();
  }
