#include "Tlc5940.h"

float tempo = 4000;//                       5                                 10                                         17                                                                   27                                                               37                                              44                                                    52  
int notasNocheDePaz[] =    {15,   7,     9,     7,    4,   15,       7,     9,     7,    4,   15,       2,   2,    11,  11,   12,  12,   7,          9,   9,    12,    11,    9,    7,    9,      7,    4,   15,        9,   9,    12,    11,    9,    7,    9,      7,   4,   15,       2,   2,    5,     2,     11,   0,    4,       12,     7,     4,    7,     5,     2,    0,    15};                                        
float dursNocheDePaz[] =   {0.75, 0.375, 0.125, 0.25, 0.5, 0.25,     0.375, 0.125, 0.25, 0.5, 0.25,     0.5, 0.25, 0.5, 0.25, 0.5, 0.25, 0.75,       0.5, 0.25, 0.375, 0.125, 0.25, 0.375, 0.125, 0.25, 0.5, 0.25,      0.5, 0.25, 0.375, 0.125, 0.25, 0.375, 0.125, 0.25, 0.5, 0.25,    0.5, 0.25, 0.375, 0.125, 0.25, 0.75, 0.5,     0.375, 0.125, 0.25, 0.375, 0.125, 0.25, 0.75, 0.75};                                                                


int notasNinoDelTambor[] =  {15,   0,    2,    4,   4,    4,    5,     4,     5,    4,    15,       15,   0,    0,    2,    4,    4,    4,    4,    5,     4,     5,    4,    15,           15,   2,    4,    5,    7,    7,    7,    9,    7,     5,     4,    2,    15,           15,   2,    4,    5,    7,    7,    7,    9,    7,     9,     7,    5,           9,     7,     5,    4,           7,     5,     4,    2,    15,         15,   0,    2,    4,   4,    4,    5,     4,     5,    4,    15,        2,     0,     2,    0,    15   };
float dursNinoDelTambor[] = {0.75, 0.75, 0.25, 0.5, 0.25, 0.25, 0.125, 0.125, 0.25, 1.25, 0.25,     0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.125, 0.125, 0.25, 1.25, 0.25,         0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.125, 0.125, 0.25, 1.25, 0.25,         0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.125, 0.125, 0.25, 0.5,         0.125, 0.125, 0.25, 0.5,         0.125, 0.125, 0.25, 1.25, 0.25,       0.75, 0.75, 0.25, 0.5, 0.25, 0.25, 0.125, 0.125, 0.25, 1.25, 0.25,      0.125, 0.125, 0.25, 1.25, 0.25};

float silencio, dur, legato;

void setup() {
  Tlc.init();
}

void loop() {

  encender(); // Escala descendente
  delay(1000);



// NOCHE DE PAZ

  for(int i = 0; i <= (sizeof(notasNocheDePaz)/2) - 1; i++) {
    silencio = tempo * dursNocheDePaz[i];
    dur = tempo * dursNocheDePaz[i] * (0.75);
    legato = tempo * dursNocheDePaz[i] * (0.25);

      Tlc.set(notasNocheDePaz[i],4080); 
      Tlc.update();
      delay(int(dur));
      Tlc.set(notasNocheDePaz[i],0); 
      Tlc.update();
      delay(int(legato));
  }


 encender();
  delay(1000);
  
// NIÑO DEL TAMBOR

  for(int i = 0; i <= (sizeof(notasNinoDelTambor)/2) - 1; i++) {
    silencio = tempo * dursNinoDelTambor[i];
    dur = tempo * dursNinoDelTambor[i] * (0.75);
    legato = tempo * dursNinoDelTambor[i] * (0.25);

      Tlc.set(notasNinoDelTambor[i],4080); 
      Tlc.update();
      delay(int(dur));
      Tlc.set(notasNinoDelTambor[i],0); 
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
  for (int i = 12; i >= -1; i--) {
    Tlc.set(i + 1, 0);
  Tlc.update();
    delay(250);
    Tlc.set(i, 4080);
  Tlc.update();
    delay(250);
  }
}
