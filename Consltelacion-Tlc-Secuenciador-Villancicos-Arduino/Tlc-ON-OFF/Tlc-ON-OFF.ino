#include "Tlc5940.h"

void setup() {
  Tlc.init();
}

void loop() {
  for(int i=0; i<=15; i++) {
      Tlc.set(i,4080);  //  rango de 0 a 4080
  }
 delay(500); 
 Tlc.update();   
        
    for(int i=0; i<=15; i++) {
        Tlc.set(i,0); 
    }
    delay(500); 
    Tlc.update(); 
  }
