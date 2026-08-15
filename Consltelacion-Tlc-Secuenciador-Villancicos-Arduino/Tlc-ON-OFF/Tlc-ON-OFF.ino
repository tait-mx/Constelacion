#include "Tlc5940.h"

void setup() {
  Tlc.init();
}

void loop() {
  for(int i=0; i<=15; i++) {
      Tlc.set(i,4080);  //  rango de 0 a 4080
  }
      Tlc.update(); 
 delay(500);   
        
    for(int i=0; i<=15; i++) {
        Tlc.set(i,0); 
    }
    Tlc.update(); 
    delay(500); 
  }
