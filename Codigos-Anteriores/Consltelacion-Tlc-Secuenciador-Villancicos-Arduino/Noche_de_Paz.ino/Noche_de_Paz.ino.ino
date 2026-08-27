#include "Tlc5940.h"

float tempo = 4000;//                       5                                 10                                         17                                                                   27                                                               37                                              44                                                    52  
int notas[] =    {15,   7,     9,     7,    4,   15,       7,     9,     7,    4,   15,       2,   2,    11,  11,   12,  12,   7,          9,   9,    12,    11,    9,    7,    9,      7,    4,   15,        9,   9,    12,    11,    9,    7,    9,      7,   4,   15,       2,   2,    5,     2,     11,   0,    4,       12,     7,     4,    7,     5,     2,    0,    15};                                        
float durs[] =   {0.75, 0.375, 0.125, 0.25, 0.5, 0.25,     0.375, 0.125, 0.25, 0.5, 0.25,     0.5, 0.25, 0.5, 0.25, 0.5, 0.25, 0.75,       0.5, 0.25, 0.375, 0.125, 0.25, 0.375, 0.125, 0.25, 0.5, 0.25,      0.5, 0.25, 0.375, 0.125, 0.25, 0.375, 0.125, 0.25, 0.5, 0.25,    0.5, 0.25, 0.375, 0.125, 0.25, 0.75, 0.5,     0.375, 0.125, 0.25, 0.375, 0.125, 0.25, 0.75, 0.75};                                                                


float silencio, dur, legato;

void setup() {
  Tlc.init();
  Serial.begin(9600);
}

void loop() {

  encender();
  apagar();

  for(int i = 0; i <= (sizeof(notas)/2) - 1; i++) {
    silencio = tempo * durs[i];
    dur = tempo * durs[i] * (0.75);
    legato = tempo * durs[i] * (0.25);

      Tlc.set(notas[i],4080); 
      Tlc.update();
      delay(int(dur));
      Tlc.set(notas[i],0); 
      Tlc.update();
      delay(int(legato));
  }
  }
  
void apagar() {
  for (int i = 0; i <= 15; i++) {
    Tlc.set(i, 0);
    delay(10);
  }
  Tlc.update();
}

  
void encender() {
  for (int i = 0; i <= 15; i++) {
    Tlc.set(i, 4080);
    delay(500);
  }
  Tlc.update();
}
