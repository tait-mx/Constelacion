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

Función de los botones
  bot1 = STOP
  bot2 = PLAY
  bot3 = REWIND
  bot4 = FORWARD
*/


#include <LiquidCrystal.h>
#include <SoftwareSerial.h>
#include "DFRobotDFPlayerMini.h"

const int rs = 13, en = 12, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
int bot1, bot2, bot3, bot4;
int botAnt1 = 0;
int botAnt2 = 0;
int botAnt3 = 0;
int botAnt4 = 0;

int track = 0;
int noTracks = 2;
int trackMasUno = 1;
String nombres[] = {"Noche de Paz", "Nino del Tambor"};


SoftwareSerial DFPlayerSerial(10, 11);  //Rx, Tx
DFRobotDFPlayerMini myDFPlayer;

void setup() {
  for (int i = 6; i <= 9; i++) {
    pinMode(i, INPUT);
  }
  
  lcd.begin(16, 2);
  lcd.print("Constelacion");
  lcd.setCursor(0,1);
  lcd.print("Navidad");
  
  Serial.begin(9600);
  DFPlayerSerial.begin(9600);
  myDFPlayer.begin(DFPlayerSerial);
  myDFPlayer.volume(30);   //De 0 a 30
  myDFPlayer.reset();
}

void loop() {
  bot1 = digitalRead(6); // STOP
  bot2 = digitalRead(7); // PLAY
  bot3 = digitalRead(8); // REWIND
  bot4 = digitalRead(9); // FORWARD

  /*
    if ((bot1 != botAnt1) || (bot2 != botAnt2) || (bot3 != botAnt3) || (bot4 != botAnt4)) {
      Serial.println((String) "Boton1 = " + bot1 + " Boton2 = " + bot2 + " Boton3 = " + bot3 + " Boton4 = " + bot4);
    }
  */

 // STOP
  if (bot1 != botAnt1) {
    myDFPlayer.reset();
    lcd.clear();
    lcd.begin(16,2);
    lcd.print("STOP");
  }
   // PLAY
  if (bot2 != botAnt2) {
    Serial.println((String) "track " + track + " tracMasUno " + trackMasUno);
    myDFPlayer.playMp3Folder(trackMasUno);
    lcd.clear();
    lcd.begin(16,2);
    lcd.print("PLAY");
    lcd.setCursor(0,1);
    lcd.print(nombres[track]);
  }
     // REWIND
  if (bot3 != botAnt3) {
    track = abs(track - 1) % noTracks;
    trackMasUno = track + 1;
    Serial.println((String) "track " + track + " tracMasUno " + trackMasUno);
    myDFPlayer.playMp3Folder(trackMasUno);
    lcd.clear();
    lcd.begin(16,2);
    lcd.print("REW");
    lcd.setCursor(0,1);
    lcd.print(nombres[track]);
  }
   // FORWARD
  if (bot4 != botAnt4) {
    track = (track + 1) % noTracks;
    trackMasUno = track + 1;
    Serial.println((String) "track " + track + " tracMasUno " + trackMasUno);
    myDFPlayer.playMp3Folder(trackMasUno);
    lcd.clear();
    lcd.begin(16,2);
    lcd.print("FWD");
    lcd.setCursor(0,1);
    lcd.print(nombres[track]);
  }

  botAnt1 = bot1;
  botAnt2 = bot2;
  botAnt3 = bot3;
  botAnt4 = bot4;
}
