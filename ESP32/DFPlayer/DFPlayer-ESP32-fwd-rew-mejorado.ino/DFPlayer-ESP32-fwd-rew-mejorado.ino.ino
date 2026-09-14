#include <Arduino.h>
#include <ezButton.h>
#include "DFRobotDFPlayerMini.h"

// Definición exacta de tus pines de botones físicos:
#define BTN_RESET_PIN 27
#define BTN_PLAY_PIN  12
#define BTN_PREV_PIN  14
#define BTN_NEXT_PIN  13

// Definición de pines para UART2 (DFPlayer)
#define RX2_PIN 16  // Conectar al TX del DFPlayer
#define TX2_PIN 17  // Conectar al RX del DFPlayer (con resistencia de 1k en serie)

// Instancias de botones con filtrado antirrebote
ezButton btnReset(BTN_RESET_PIN);
ezButton btnPlay(BTN_PLAY_PIN);
ezButton btnPrev(BTN_PREV_PIN);
ezButton btnNext(BTN_NEXT_PIN);

// Instancia de hardware serial y DFPlayer
HardwareSerial DFPlayerSerial(2); // UART2 nativo del ESP32
DFRobotDFPlayerMini myDFPlayer;

int currentTrack = 1;
const int totalTracks = 3; // Ajustado a tus 3 pistas
bool isPlaying = false;    // Rastra si el reproductor está activo o en silencio

void setup() {
  Serial.begin(115200);

  // Configuración del antirrebote (50ms)
  btnReset.setDebounceTime(50);
  btnPlay.setDebounceTime(50);
  btnPrev.setDebounceTime(50);
  btnNext.setDebounceTime(50);

  // Inicializar comunicación serial por hardware con el DFPlayer
  DFPlayerSerial.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);

  Serial.println(F("Inicializando DFPlayer Mini..."));
  if (!myDFPlayer.begin(DFPlayerSerial)) {
    Serial.println(F("Error: Comprueba las conexiones o la tarjeta SD."));
    while (true); // Detener si hay fallo
  }

  myDFPlayer.volume(20); // Volumen de 0 a 30
  Serial.println(F("DFPlayer Mini listo."));
}

void loop() {
  // Actualizar estado de los botones
  btnReset.loop();
  btnPlay.loop();
  btnPrev.loop();
  btnNext.loop();

  // -------------------------------------------------------------------
  // BOTÓN 1 (GPIO 27): STOP / PAUSA
  // -------------------------------------------------------------------
  if (btnReset.isPressed()) {
    Serial.println(F("Acción: Stop / Pausa"));
    myDFPlayer.pause();
    isPlaying = false; // Marcamos que está detenido
  }

  // -------------------------------------------------------------------
  // BOTÓN 2 (GPIO 12): PLAY
  // -------------------------------------------------------------------
  if (btnPlay.isPressed()) {
    Serial.print(F("Acción: Play Pista "));
    Serial.println(currentTrack);
    myDFPlayer.playMp3Folder(currentTrack);
    isPlaying = true; // Marcamos que comenzó a reproducir
  }

  // -------------------------------------------------------------------
  // BOTÓN 3 (GPIO 14): ANTERIOR (REW)
  // -------------------------------------------------------------------
  if (btnPrev.isPressed()) {
    currentTrack--;
    if (currentTrack < 1) {
      currentTrack = totalTracks; // Navegación circular
    }

    Serial.print(F("Pista seleccionada: "));
    Serial.println(currentTrack);

    if (isPlaying) {
      Serial.println(F("-> Reproduciendo pista anterior..."));
      myDFPlayer.playMp3Folder(currentTrack);
    } else {
      Serial.println(F("-> Pista cambiada en silencio. Presiona PLAY."));
    }
  }

  // -------------------------------------------------------------------
  // BOTÓN 4 (GPIO 13): SIGUIENTE (FWD)
  // -------------------------------------------------------------------
  if (btnNext.isPressed()) {
    currentTrack++;
    if (currentTrack > totalTracks) {
      currentTrack = 1; // Navegación circular
    }

    Serial.print(F("Pista seleccionada: "));
    Serial.println(currentTrack);

    if (isPlaying) {
      Serial.println(F("-> Reproduciendo pista siguiente..."));
      myDFPlayer.playMp3Folder(currentTrack);
    } else {
      Serial.println(F("-> Pista cambiada en silencio. Presiona PLAY."));
    }
  }
}
