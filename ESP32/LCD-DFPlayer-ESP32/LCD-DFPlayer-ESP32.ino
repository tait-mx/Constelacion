#include <Arduino.h>
#include <LiquidCrystal.h>
#include <ezButton.h>
#include <DFRobotDFPlayerMini.h>

// --- Configuración del LCD1602 (Modo 4 bits) ---
// LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);
const int PIN_RS = 19;
const int PIN_EN = 23;
const int PIN_D4 = 18;
const int PIN_D5 = 5;
const int PIN_D6 = 4;
const int PIN_D7 = 2;

LiquidCrystal lcd(PIN_RS, PIN_EN, PIN_D4, PIN_D5, PIN_D6, PIN_D7);

// --- Configuración de Botones ---
ezButton btnStop(27);
ezButton btnPlay(12);
ezButton btnRew(14);
ezButton btnFwd(13);

// --- Configuración del DFPlayer Mini (UART2) ---
HardwareSerial mySerial2(2); // Usar UART2 de hardware
DFRobotDFPlayerMini myDFPlayer;

// --- Estado del Reproductor ---
int currentTrack = 1;
int totalTracks = 99; // Límite de pistas estimado
bool isPlaying = false;

// --- Función para actualizar la pantalla ---
void updateLCD() {
  lcd.setCursor(0, 0);
  lcd.print("Pista: ");
  if (currentTrack < 10) lcd.print("0"); // Formato dos dígitos (01, 02...)
  lcd.print(currentTrack);
  lcd.print("       "); // Limpiar caracteres residuales

  lcd.setCursor(0, 1);
  if (isPlaying) {
    lcd.print("Estado: PLAY   ");
  } else {
    lcd.print("Estado: STOP   ");
  }
}

void setup() {
  // Inicializar LCD
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");

  // Configurar tiempo de antirrebote para los botones
  btnStop.setDebounceTime(50);
  btnPlay.setDebounceTime(50);
  btnRew.setDebounceTime(50);
  btnFwd.setDebounceTime(50);

  // Inicializar UART2 para DFPlayer (RX=16, TX=17)
  mySerial2.begin(9600, SERIAL_8N1, 16, 17);

  if (!myDFPlayer.begin(mySerial2)) {
    lcd.setCursor(0, 1);
    lcd.print("Error DFPlayer ");
    while (true); // Detener ejecución si falla el DFPlayer
  }

  myDFPlayer.volume(20); // Volumen inicial (0 a 30)

  // Mensaje inicial en pantalla
  lcd.clear();
  updateLCD();
}

void loop() {
  // Actualizar lectura de los botones
  btnStop.loop();
  btnPlay.loop();
  btnRew.loop();
  btnFwd.loop();

  // --- BOTÓN PLAY ---
  if (btnPlay.isPressed()) {
    myDFPlayer.play(currentTrack);
    isPlaying = true;
    updateLCD();
  }

  // --- BOTÓN STOP / PAUSE ---
  if (btnStop.isPressed()) {
    myDFPlayer.pause();
    isPlaying = false;
    updateLCD();
  }

  // --- BOTÓN FWD (Siguiente) ---
  if (btnFwd.isPressed()) {
    currentTrack++;
    if (currentTrack > totalTracks) currentTrack = 1;

    if (isPlaying) {
      myDFPlayer.play(currentTrack); // Cambia y reproduce inmediatamente
    }
    updateLCD();
  }

  // --- BOTÓN REW (Anterior) ---
  if (btnRew.isPressed()) {
    currentTrack--;
    if (currentTrack < 1) currentTrack = totalTracks;

    if (isPlaying) {
      myDFPlayer.play(currentTrack); // Cambia y reproduce inmediatamente
    }
    updateLCD();
  }
}
