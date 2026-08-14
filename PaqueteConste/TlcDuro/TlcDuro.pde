#include "Tlc5940.h"

int inByte;
int led = 0;

void setup() {
  Tlc.init(0);
}

void loop() {
  
  for(int i=0; i<=47; i++)
  {
      Tlc.set(0,0);
      Tlc.update();
  }
  
}

