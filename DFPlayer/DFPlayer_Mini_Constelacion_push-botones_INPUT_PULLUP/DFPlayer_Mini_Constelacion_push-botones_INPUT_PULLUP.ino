/*
   myDFPlayer.begin(DFPlayerSerial)
   myDFPlayer.volume(volume); 0-30
   myDFPlayer.play();
   myDFPlayer.play(5);  Reproduce el archivo 0005
   myDFPlayer.next();
   myDFPlayer.previous();
   myDFPlayer.pause();
   myDFPlayer.start();
   myDFPlayer.randomAll();
   myDFPlayer.enableLoop();
   myDFPlayer.disableLoop();
*/
int bot1, bot2, bot3, bot4;
int botAnt1 = 0;
int botAnt2 = 0;
int botAnt3 = 0;
int botAnt4 = 0;

int track = 0;
int noTracks = 2;
int trackMasUno = 1;


#include <SoftwareSerial.h>
#include "DFRobotDFPlayerMini.h"

//SoftwareSerial DFPlayerSerial(10, 11);  //Rx, Tx
SoftwareSerial DFPlayerSerial(26, 27);  //Rx, Tx
DFRobotDFPlayerMini myDFPlayer;

void setup() {
  for (int i = 22; i <= 26; i++) {
    pinMode(i, INPUT_PULLUP);
  }
  Serial.begin(9600);
  DFPlayerSerial.begin(9600);
  myDFPlayer.begin(DFPlayerSerial);
  myDFPlayer.volume(30);   //De 0 a 30
  myDFPlayer.reset();
}

void loop() {
  /*
  bot1 = abs((digitalRead(6) -1);
  bot2 = abs((digitalRead(7) -1);
  bot3 = abs((digitalRead(8) -1);
  bot4 = abs((digitalRead(9) -1);
*/
  
  bot1 = abs(digitalRead(22) -1);
  bot2 = abs(digitalRead(23) -1);
  bot3 = abs(digitalRead(24) -1);
  bot4 = abs(digitalRead(25) -1);

  /*
    if ((bot1 != botAnt1) || (bot2 != botAnt2) || (bot3 != botAnt3) || (bot4 != botAnt4)) {
      Serial.println((String) "Boton1 = " + bot1 + " Boton2 = " + bot2 + " Boton3 = " + bot3 + " Boton4 = " + bot4);
    }
  */

  if (bot1 != botAnt1) {
    if(bot1 == 1) {
    myDFPlayer.reset();
    }
  }
  if (bot2 != botAnt2) {
    if(bot2 == 1) {
    Serial.println((String) "track " + track + " tracMasUno " + trackMasUno);
    myDFPlayer.playMp3Folder(trackMasUno);
    }
  }
  if (bot3 != botAnt3) {
    if(bot3 == 1) {
    track = abs(track - 1) % noTracks;
    trackMasUno = track + 1;
    Serial.println((String) "track " + track + " tracMasUno " + trackMasUno);
    myDFPlayer.playMp3Folder(trackMasUno);
    }
  }
  if (bot4 != botAnt4) {
    if(bot4 == 1) {
    track = (track + 1) % noTracks;
    trackMasUno = track + 1;
    Serial.println((String) "track " + track + " tracMasUno " + trackMasUno);
    myDFPlayer.playMp3Folder(trackMasUno);
    }
  }

  botAnt1 = bot1;
  botAnt2 = bot2;
  botAnt3 = bot3;
  botAnt4 = bot4;
}
