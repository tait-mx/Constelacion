/*
   Combined sketch: TLC5940 LED villancico sequencer + DFPlayer Mini button control.

   Buttons (INPUT_PULLUP):
     bot1 (pin 22) = STOP   -> reset DFPlayer, turn LEDs off and pause sequence
     bot2 (pin 23) = PLAY   -> play current track and jump LEDs to that song
     bot3 (pin 24) = REWIND -> previous track + matching LED sequence
     bot4 (pin 25) = FORWARD-> next track + matching LED sequence

   DFPlayer on SoftwareSerial(26, 27).  TLC5940 uses the standard Mega pins
   (9/10/11/12/50/51/52/53), no conflict with pins 22-27.

   The LED sequencer is non-blocking (millis() driven) so the buttons are
   always polled while the villancicos play.
*/

#include <SoftwareSerial.h>
#include "DFRobotDFPlayerMini.h"
#include "Tlc5940.h"

// ---------------------------------------------------------------------------
// Villancico data
// ---------------------------------------------------------------------------

float tempo = 4000;   // ms per whole note

//                       5                                 10                                         17                                                                   27                                                               37                                              44                                                    52
int notasNocheDePaz[] =    {15,   7,     9,     7,    4,   15,       7,     9,     7,    4,   15,       2,   2,    11,  11,   12,  12,   7,          9,   9,    12,    11,    9,    7,    9,      7,    4,   15,        9,   9,    12,    11,    9,    7,    9,      7,   4,   15,       2,   2,    5,     2,     11,   0,    4,       12,     7,     4,    7,     5,     2,    0,    15};
float dursNocheDePaz[] =   {0.75, 0.375, 0.125, 0.25, 0.5, 0.25,     0.375, 0.125, 0.25, 0.5, 0.25,     0.5, 0.25, 0.5, 0.25, 0.5, 0.25, 0.75,       0.5, 0.25, 0.375, 0.125, 0.25, 0.375, 0.125, 0.25, 0.5, 0.25,      0.5, 0.25, 0.375, 0.125, 0.25, 0.375, 0.125, 0.25, 0.5, 0.25,    0.5, 0.25, 0.375, 0.125, 0.25, 0.75, 0.5,     0.375, 0.125, 0.25, 0.375, 0.125, 0.25, 0.75, 0.75};

int notasNinoDelTambor[] =  {15,   0,    2,    4,   4,    4,    5,     4,     5,    4,    15,       15,   0,    0,    2,    4,    4,    4,    4,    5,     4,     5,    4,    15,           15,   2,    4,    5,    7,    7,    7,    9,    7,     5,     4,    2,    15,           15,   2,    4,    5,    7,    7,    7,    9,    7,     9,     7,    5,           9,     7,     5,    4,           7,     5,     4,    2,    15,         15,   0,    2,    4,   4,    4,    5,     4,     5,    4,    15,        2,     0,     2,    0,    15   };
float dursNinoDelTambor[] = {0.75, 0.75, 0.25, 0.5, 0.25, 0.25, 0.125, 0.125, 0.25, 1.25, 0.25,     0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.125, 0.125, 0.25, 1.25, 0.25,         0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.125, 0.125, 0.25, 1.25, 0.25,         0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.125, 0.125, 0.25, 0.5,         0.125, 0.125, 0.25, 0.5,         0.125, 0.125, 0.25, 1.25, 0.25,       0.75, 0.75, 0.25, 0.5, 0.25, 0.25, 0.125, 0.125, 0.25, 1.25, 0.25,      0.125, 0.125, 0.25, 1.25, 0.25};

struct Song {
  const int* notas;
  const float* durs;
  int len;
};

Song songs[] = {
  {notasNocheDePaz,    dursNocheDePaz,    sizeof(notasNocheDePaz)    / sizeof(int)},
  {notasNinoDelTambor, dursNinoDelTambor, sizeof(notasNinoDelTambor) / sizeof(int)}
};

// ---------------------------------------------------------------------------
// Non-blocking LED sequencer state
// ---------------------------------------------------------------------------

int activeSong = 0;
int noteIndex = 0;
unsigned long noteStartMs = 0;
unsigned long offStartMs = 0;
float dur = 0;       // LED on time for current note
float legato = 0;    // LED off time for current note

enum NoteState { NOTE_START, NOTE_ON, NOTE_OFF };
NoteState noteState = NOTE_START;

// Power-on intro (descending scale)
bool introDone = false;
int introStep = 12;
unsigned long introStartMs = 0;

bool stopped = false;

// ---------------------------------------------------------------------------
// DFPlayer globals
// ---------------------------------------------------------------------------

int bot1, bot2, bot3, bot4;
int botAnt1 = 0;
int botAnt2 = 0;
int botAnt3 = 0;
int botAnt4 = 0;

int track = 0;
int noTracks = 2;
int trackMasUno = 1;

SoftwareSerial DFPlayerSerial(26, 27);   // Rx, Tx
DFRobotDFPlayerMini myDFPlayer;

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void setup() {
  Tlc.init();
  for (int i = 22; i <= 26; i++) {
    pinMode(i, INPUT_PULLUP);
  }
  Serial.begin(9600);
  DFPlayerSerial.begin(9600);
  myDFPlayer.begin(DFPlayerSerial);
  myDFPlayer.volume(30);   // De 0 a 30
  myDFPlayer.reset();
}

// ---------------------------------------------------------------------------
// Main loop: buttons and LED sequencer never block
// ---------------------------------------------------------------------------

void loop() {
  handleButtons();
  updateLEDsequence();
}

// ---------------------------------------------------------------------------
// Buttons
// ---------------------------------------------------------------------------

void handleButtons() {
  bot1 = abs(digitalRead(22) - 1);
  bot2 = abs(digitalRead(23) - 1);
  bot3 = abs(digitalRead(24) - 1);
  bot4 = abs(digitalRead(25) - 1);

  if (bot1 != botAnt1) {
    if (bot1 == 1) {
      myDFPlayer.reset();
      stopSequence();
    }
  }
  if (bot2 != botAnt2) {
    if (bot2 == 1) {
      Serial.println((String) "track " + track + " tracMasUno " + trackMasUno);
      myDFPlayer.playMp3Folder(trackMasUno);
      jumpToSong(track);
    }
  }
  if (bot3 != botAnt3) {
    if (bot3 == 1) {
      track = abs(track - 1) % noTracks;
      trackMasUno = track + 1;
      Serial.println((String) "track " + track + " tracMasUno " + trackMasUno);
      myDFPlayer.playMp3Folder(trackMasUno);
      jumpToSong(track);
    }
  }
  if (bot4 != botAnt4) {
    if (bot4 == 1) {
      track = (track + 1) % noTracks;
      trackMasUno = track + 1;
      Serial.println((String) "track " + track + " tracMasUno " + trackMasUno);
      myDFPlayer.playMp3Folder(trackMasUno);
      jumpToSong(track);
    }
  }

  botAnt1 = bot1;
  botAnt2 = bot2;
  botAnt3 = bot3;
  botAnt4 = bot4;
}

// Start playing the given song from the beginning (LED side).
void jumpToSong(int songIdx) {
  activeSong = songIdx;
  noteIndex = 0;
  noteState = NOTE_START;
  introDone = true;   // skip the power-on scale when a track is selected
  stopped = false;
}

// Turn all LEDs off and pause the sequence.
void stopSequence() {
  apagar();
  stopped = true;
}

// ---------------------------------------------------------------------------
// Non-blocking LED sequencer
// ---------------------------------------------------------------------------

void updateLEDsequence() {
  if (stopped) return;

  if (!introDone) {
    runIntro();
    return;
  }

  Song* s = &songs[activeSong];

  switch (noteState) {
    case NOTE_START: {
      Tlc.set(s->notas[noteIndex], 4080);
      Tlc.update();
      dur = tempo * s->durs[noteIndex] * 0.75;
      legato = tempo * s->durs[noteIndex] * 0.25;
      noteStartMs = millis();
      noteState = NOTE_ON;
      break;
    }
    case NOTE_ON: {
      if ((unsigned long)(millis() - noteStartMs) >= (unsigned long)dur) {
        Tlc.set(s->notas[noteIndex], 0);
        Tlc.update();
        offStartMs = millis();
        noteState = NOTE_OFF;
      }
      break;
    }
    case NOTE_OFF: {
      if ((unsigned long)(millis() - offStartMs) >= (unsigned long)legato) {
        noteIndex++;
        if (noteIndex >= s->len) noteIndex = 0;   // loop the song
        noteState = NOTE_START;
      }
      break;
    }
  }
}

// Power-on descending scale animation, runs once.
void runIntro() {
  if (millis() - introStartMs >= 250) {
    Tlc.set(introStep + 1, 0);
    Tlc.set(introStep, 4080);
    Tlc.update();
    introStep--;
    introStartMs = millis();
    if (introStep < 0) {
      introDone = true;
      apagar();
      noteIndex = 0;
      noteState = NOTE_START;
    }
  }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void apagar() {
  for (int i = 0; i <= 15; i++) {
    Tlc.set(i, 0);
  }
  Tlc.update();
}
