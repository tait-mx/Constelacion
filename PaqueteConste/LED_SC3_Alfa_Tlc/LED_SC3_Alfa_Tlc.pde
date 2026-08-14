#include "Tlc5940.h"

int inByte;
int led = 0;

void setup() {
  Serial.begin(9600);
  Tlc.init();
}

void loop() {

  if(Serial.available() >0) {

    inByte=Serial.read();

    if(inByte==255) {
      led=0;
    }
    else
    {
      Tlc.set((led%32),inByte*16);
      Tlc.update();
      led++;
    }
  }
}

