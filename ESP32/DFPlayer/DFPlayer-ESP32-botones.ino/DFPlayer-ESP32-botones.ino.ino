#include <Arduino.h>
#include <ezButton.h>
#include "DFRobotDFPlayerMini.h"

// Definición de pines para los 4 botones
#define BTN_RESET_PIN 27
#define BTN_PLAY_PIN  12
#define BTN_PREV_PIN  14
#define BTN_NEXT_PIN  13

// Definición de pines para UART2 (DFPlayer)
#define RX2_PIN 16  // Conectar al TX del DFPlayer (con o sin resistencia de 1k)
#define TX2_PIN 17  // Conectar al RX del DFPlayer (con resistencia de 1k en serie)

// Instancias de botones con filtrado antirrebote (debounce de 50ms)
ezButton btnReset(BTN_RESET_PIN);
ezButton btnPlay(BTN_PLAY_PIN);
ezButton btnPrev(BTN_PREV_PIN);
ezButton btnNext(BTN_NEXT_PIN);

// Instancia de hardware serial y DFPlayer
HardwareSerial DFPlayerSerial(2); // Usamos UART2 nativo del ESP32
DFRobotDFPlayerMini myDFPlayer;

int currentTrack = 1;
const int totalTracks = 3; // Ajusta según el número de pistas en tu carpeta SD

void setup() {
  Serial.begin(115200);
  
  // Configuración del antirrebote para botones
  btnReset.setDebounceTime(50);
  btnPlay.setDebounceTime(50);
  btnPrev.setDebounceTime(50);
  btnNext.setDebounceTime(50);

  // Inicializar comunicación serial por hardware con el DFPlayer
  // Parametros: Baudrate, Configuración, RX, TX
  DFPlayerSerial.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);

  Serial.println(F("Inicializando DFPlayer Mini..."));
  if (!myDFPlayer.begin(DFPlayerSerial)) {
    Serial.println(F("Error: Comprueba las conexiones o la tarjeta SD."));
    while (true); // Detener si hay fallo
  }

  myDFPlayer.volume(20); // Volumen de 0 a 30 (20 es seguro para evitar distorsión)
  Serial.println(F("DFPlayer Mini listo."));
}

void loop() {
  // Actualizar estado de los botones
  btnReset.loop();
  btnPlay.loop();
  btnPrev.loop();
  btnNext.loop();

  // Botón 1: Reset / Stop
  if (btnReset.isPressed()) {
    Serial.println(F("Acción: Reset Módulo"));
    myDFPlayer.reset();
  }

  // Botón 2: Reproducir pista actual
  if (btnPlay.isPressed()) {
    Serial.print(F("Acción: Play Pista "));
    Serial.println(currentTrack);
    myDFPlayer.playMp3Folder(currentTrack); // Reproduce carpeta SD:/MP3/0001.mp3
  }

  // Botón 3: Pista anterior (Previous)
  if (btnPrev.isPressed()) {
    currentTrack--;
    if (currentTrack < 1) {
      currentTrack = totalTracks; // Navegación circular hacia la última pista
    }
    Serial.print(F("Acción: Pista Anterior -> "));
    Serial.println(currentTrack);
    myDFPlayer.playMp3Folder(currentTrack);
  }

  // Botón 4: Pista siguiente (Next)
  if (btnNext.isPressed()) {
    currentTrack++;
    if (currentTrack > totalTracks) {
      currentTrack = 1; // Navegación circular hacia la primera pista
    }
    Serial.print(F("Acción: Pista Siguiente -> "));
    Serial.println(currentTrack);
    myDFPlayer.playMp3Folder(currentTrack);
  }
}
