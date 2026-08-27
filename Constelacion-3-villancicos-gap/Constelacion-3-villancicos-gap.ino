/*
Combined sketch: TLC5940 LED villancico sequencer + DFPlayer Mini button control.

   Buttons (INPUT_PULLUP):
     bot1 (pin 22) = STOP   -> reset DFPlayer, turn LEDs off and pause sequence
     bot2 (pin 23) = PLAY   -> play current track and jump LEDs to that song
     bot3 (pin 24) = REWIND -> previous track. Plays it if already playing; otherwise just selects it.
     bot4 (pin 25) = FORWARD-> next track. Plays it if already playing; otherwise just selects it.

   DFPlayer on SoftwareSerial(26, 27).  TLC5940 uses the standard Mega pins
   (9/10/11/12/50/51/52/53), no conflict with pins 22-27.

   The LED sequencer is non-blocking (millis() driven) so the buttons are
   always polled while the villancicos play.  Each note's LED stays lit for   the note's full duration and hands off to the next note without going dark.
*/

#include <SoftwareSerial.h>
#include "DFRobotDFPlayerMini.h"
#include "Tlc5940.h"
#include <LiquidCrystal.h>

const int rs = 7, en = 6, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Gap between notes
 


// ---------------------------------------------------------------------------
// Villancico data
// ---------------------------------------------------------------------------

#define SILENCE -1   // sentinel: no LED lit during this beat

float tempo = 4000;   // ms per whole note

//                       5                                 10                                         17                                                                   27                                                               37                                              44                                                    52
int notasNocheDePaz[] =    {7,     9,     7,    4,   SILENCE,  7,     9,     7,    4,   SILENCE,  2,   2,    11,  11,   12,  12,   7,          9,   9,    12,    11,    9,    7,    9,      7,    4,   SILENCE,   9,   9,    12,    11,    9,    7,    9,      7,   4,   SILENCE,  2,   2,    5,     2,     11,   0,    4,       12,     7,     4,    7,     5,     2,    0,    SILENCE};
float dursNocheDePaz[] =   {0.375, 0.125, 0.25, 0.5, 0.25,     0.375, 0.125, 0.25, 0.5, 0.25,     0.5, 0.25, 0.5, 0.25, 0.5, 0.25, 0.75,       0.5, 0.25, 0.375, 0.125, 0.25, 0.375, 0.125, 0.25, 0.5, 0.25,      0.5, 0.25, 0.375, 0.125, 0.25, 0.375, 0.125, 0.25, 0.5, 0.25,    0.5, 0.25, 0.375, 0.125, 0.25, 0.75, 0.5,     0.375, 0.125, 0.25, 0.375, 0.125, 0.25, 0.75, 0.75};

int notasNinoDelTambor[] =  {0,    2,    4,   4,    4,    5,     4,     5,    4,    SILENCE,  SILENCE,   0,    0,    2,    4,    4,    4,    4,    5,     4,     5,    4,    SILENCE,      SILENCE,   2,    4,    5,    7,    7,    7,    9,    7,     5,     4,    2,    SILENCE,      SILENCE,   2,    4,    5,    7,    7,    7,    9,    7,     9,     7,    5,           9,     7,     5,    4,           7,     5,     4,    2,    SILENCE,    SILENCE,   0,    2,    4,   4,    4,    5,     4,     5,    4,    SILENCE,   2,     0,     2,    0,    SILENCE};
float dursNinoDelTambor[] = {0.75, 0.25, 0.5, 0.25, 0.25, 0.125, 0.125, 0.25, 1.25, 0.25,     0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.125, 0.125, 0.25, 1.25, 0.25,         0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.125, 0.125, 0.25, 1.25, 0.25,         0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.125, 0.125, 0.25, 0.5,         0.125, 0.125, 0.25, 0.5,         0.125, 0.125, 0.25, 1.25, 0.25,       0.75, 0.75, 0.25, 0.5, 0.25, 0.25, 0.125, 0.125, 0.25, 1.25, 0.25,      0.125, 0.125, 0.25, 1.25, 0.25};

// index:     0    10    20    30    40    50
int notasJoyToTheWorld[] = {
  0, 11, 9, 7, 5, 4, 2, 0, 7, 9,
  9, 11, 11, 0, 0, 0, 11, 9, 7, 7,
  5, 4, 0, 0, 11, 9, 7, 7, 5, 4,
  4, 4, 4, 4, 4, 5, 7, 5, 4, 2,
  2, 2, 2, 4, 5, 4, 2, 0, 0, 9,
  7, 5, 4, 5, 4, 2, 0
};

// index:     0    10    20    30    40    50
float dursJoyToTheWorld[] = {
  0.25, 0.1875, 0.0625, 0.375, 0.125, 0.25, 0.25, 0.375, 0.125, 0.375,
  0.125, 0.375, 0.125, 0.375, 0.125, 0.125, 0.125, 0.125, 0.125, 0.1875,
  0.0625, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.1875, 0.0625, 0.125,
  0.125, 0.125, 0.125, 0.125, 0.0625, 0.0625, 0.375, 0.0625, 0.0625, 0.125,
  0.125, 0.125, 0.0625, 0.0625, 0.375, 0.0625, 0.0625, 0.125, 0.25, 0.125,
  0.1875, 0.0625, 0.125, 0.125, 0.25, 0.25, 0.5
};

struct Song {
  const int* notas;
  const float* durs;
  int len;
  const char* nameLine1;   // LCD row 0
  const char* nameLine2;   // LCD row 1 (empty string if unused)
};

Song songs[] = {
  {notasNocheDePaz,    dursNocheDePaz,    sizeof(notasNocheDePaz)    / sizeof(int), "Noche de Paz",  ""},
  {notasNinoDelTambor, dursNinoDelTambor, sizeof(notasNinoDelTambor) / sizeof(int), "El Nino del",   "Tambor"},
  {notasJoyToTheWorld, dursJoyToTheWorld, sizeof(notasJoyToTheWorld) / sizeof(int), "Joy to the",    "World"}
};

// ---------------------------------------------------------------------------
// Non-blocking LED sequencer state
// ---------------------------------------------------------------------------

int activeSong = 0;
int noteIndex = 0;
int currentChannel = -1;   // LED channel currently lit (-1 = none)
unsigned long noteStartMs = 0;
float dur = 0;             // full duration of the current note

enum NoteState { NOTE_START, NOTE_ON, NOTE_GAP };
NoteState noteState = NOTE_START;

const unsigned long NOTE_GAP_MS = 60;   // LED-off blip when a note repeats, to show the re-attack
unsigned long gapStartMs = 0;
unsigned long gapMs = 0;  

// Power-on intro (descending scale)
bool introDone = false;
int introStep = 12;
unsigned long introStartMs = 0;

bool stopped = true;

// ---------------------------------------------------------------------------
// DFPlayer globals
// ---------------------------------------------------------------------------

int bot1, bot2, bot3, bot4;
int botAnt1 = 0;
int botAnt2 = 0;
int botAnt3 = 0;
int botAnt4 = 0;

int track = 0;
int noTracks = 3;
int trackMasUno = 1;
unsigned long lastDebounceMs = 0;
const unsigned long debounceDelay = 50;

SoftwareSerial DFPlayerSerial(26, 27);   // Rx, Tx
DFRobotDFPlayerMini myDFPlayer;

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void setup() {
  Tlc.init();
  for (int i = 22; i <= 25; i++) {
    pinMode(i, INPUT_PULLUP);
  }
  lcd.begin(16, 2);
  Serial.begin(9600);
  DFPlayerSerial.begin(9600);
  myDFPlayer.begin(DFPlayerSerial);
  myDFPlayer.volume(30);   // De 0 a 30
  myDFPlayer.reset();
  showIntroOnLCD();
  runIntroBlocking();      // power-on descending scale before waiting for PLAY
  showSongOnLCD(track);    // show the track that's cued up (track 0 at boot)
}

// ---------------------------------------------------------------------------
// Main loop: buttons and LED sequencer never block
// ---------------------------------------------------------------------------

void loop() {
  handleButtons();
  updateLEDsequence();
  Tlc.update();   // keep TLC5940 PWM refreshed every cycle
}

// ---------------------------------------------------------------------------
// Buttons
// ---------------------------------------------------------------------------

void handleButtons() {
  if (millis() - lastDebounceMs < debounceDelay) return;
  lastDebounceMs = millis();

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
      if (!stopped) {
        myDFPlayer.playMp3Folder(trackMasUno);
        jumpToSong(track);
      } else {
        selectSong(track);
      }
    }
  }
  if (bot4 != botAnt4) {
    if (bot4 == 1) {
      track = (track + 1) % noTracks;
      trackMasUno = track + 1;
      Serial.println((String) "track " + track + " tracMasUno " + trackMasUno);
      if (!stopped) {
        myDFPlayer.playMp3Folder(trackMasUno);
        jumpToSong(track);
      } else {
        selectSong(track);
      }
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
  showSongOnLCD(songIdx);
}

// Select the given song (LCD + internal state) without starting playback.
void selectSong(int songIdx) {
  activeSong = songIdx;
  showSongOnLCD(songIdx);
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
      int ch = s->notas[noteIndex];
      dur = tempo * s->durs[noteIndex];
      noteStartMs = millis();

      if (ch == SILENCE) {
        if (currentChannel >= 0) { Tlc.set(currentChannel, 0); while (Tlc.update()); }
        currentChannel = SILENCE;
        noteState = NOTE_ON;
      } else if (ch == currentChannel) {
        // Repeated note: brief LED-off blip so the re-attack is visible,
        // total note duration (dur) is unchanged.
        Tlc.set(currentChannel, 0);
        while (Tlc.update());
        gapMs = min((unsigned long)NOTE_GAP_MS, (unsigned long)(dur * 0.4));
        gapStartMs = millis();
        noteState = NOTE_GAP;
      } else {
        if (currentChannel >= 0) Tlc.set(currentChannel, 0);
        currentChannel = ch;
        Tlc.set(currentChannel, 4080);
        while (Tlc.update());
        noteState = NOTE_ON;
      }
      break;
    }
    case NOTE_GAP: {
      if ((unsigned long)(millis() - gapStartMs) >= gapMs) {
        Tlc.set(currentChannel, 4080);
        while (Tlc.update());
        noteState = NOTE_ON;
      }
      break;
    }
    case NOTE_ON: {
      if ((unsigned long)(millis() - noteStartMs) >= (unsigned long)dur) {
        noteIndex++;
        if (noteIndex >= s->len) { stopSequence(); return; }
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
    while (Tlc.update());
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

void runIntroBlocking() {
  unsigned long start = millis();
  for (int step = 12; step >= 0; step--) {
    if (step < 12) Tlc.set(step + 1, 0);
    Tlc.set(step, 4080);
    while (Tlc.update());
    start = millis();
    while (millis() - start < 250) Tlc.update();
  }
  apagar();
}

void apagar() {
  for (int i = 0; i <= 15; i++) {
    Tlc.set(i, 0);
  }
  while (Tlc.update());
  currentChannel = -1;
}

void showIntroOnLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("La Constelacion");
}

void showSongOnLCD(int songIdx) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(songs[songIdx].nameLine1);
  lcd.setCursor(0, 1);
  lcd.print(songs[songIdx].nameLine2);
}
