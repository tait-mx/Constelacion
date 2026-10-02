/*
  ESP32 (DevKit clásico) - MAESTRO "Constelación"
  Cliente del router + LEDs remotos (multi-cliente ESP32-C3)
  + DFPlayer Mini (audio) + 4 BOTONES FÍSICOS + LCD 16x2 I2C
  -------------------------------------------------------------------
  Villancicos: "Noche de Paz" (0001.mp3), "El Niño del Tambor" (0002.mp3) y
  "Joy to the World" (0003.mp3). En todo momento hay un "villancico en turno"
  (currentVillancico, 0/1/2) que determina QUÉ pista suena en el DFPlayer,
  QUÉ arreglo de LEDs se reproduce y QUÉ nombre se muestra en el LCD.
  Los tres siempre van de la mano:
    - PLAY: arranca la pista y reinicia la secuencia de LEDs desde el paso 0.
    - STOP: pausa la pista y apaga de inmediato cualquier LED encendido.
    - SIGUIENTE/ANTERIOR: cambia el villancico en turno (circular). Si ya
      sonaba, apaga el LED en turno, cambia de pista y reinicia la secuencia.
      Si estaba detenido, solo cambia la selección en silencio. En ambos
      casos el LCD actualiza el nombre.
    - Si el villancico termina solo, la secuencia se detiene hasta presionar
      Play/Siguiente/Anterior.

  LCD (LiquidCrystal_I2C, dirección 0x27, 16x2, SDA=GPIO21, SCL=GPIO22):
    Muestra únicamente el nombre del villancico seleccionado (nombres[]),
    en Play o en Stop. Se actualiza solo al arrancar y con Siguiente/Anterior.

  Mapeo de notas a índice de cliente:
    0=do 1=re 2=mi 3=fa 4=sol 5=la 6=si 7=do (octava arriba)
  SILENCE = ningún cliente recibe señal (ni parpadea el LED del maestro).

  Librerías necesarias: ezButton, DFRobotDFPlayerMini, LiquidCrystal_I2C.
*/

#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ezButton.h>
#include "DFRobotDFPlayerMini.h"

const char* ssid     = "constelacion";
const char* password = "01Constelacion01";
const uint16_t puerto = 8080;

// IP fija reservada para este maestro en el router (192.168.0.150)
IPAddress ipFija(192, 168, 0, 150);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

// Cantidad máxima de clientes ESP32-C3 que puede atender simultáneamente
const int MAX_CLIENTES = 8;

WiFiServer servidor(puerto);
WiFiClient clientes[MAX_CLIENTES];

// ---------------------------------------------------------------
// LCD 16x2 I2C
// ---------------------------------------------------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------------------------------------------------------------
// DFPLAYER MINI + BOTONES FÍSICOS
// ---------------------------------------------------------------

#define BTN_RESET_PIN 27  // Stop
#define BTN_PLAY_PIN  12  // Play
#define BTN_PREV_PIN  14  // Retroceder al villancico anterior
#define BTN_NEXT_PIN  13  // Avanzar al villancico siguiente

#define BTN_LED_PIN    25  // Push button -> enciende LED del cliente 4 mientras se mantiene
#define CLIENTE_BOTON  4   // Cliente cuyo LED controla este botón

// Pines para UART2 (DFPlayer)
#define RX2_PIN 16  // Conectar al TX del DFPlayer
#define TX2_PIN 17  // Conectar al RX del DFPlayer (con resistencia de 1k en serie)

ezButton btnReset(BTN_RESET_PIN);
ezButton btnPlay(BTN_PLAY_PIN);
ezButton btnPrev(BTN_PREV_PIN);
ezButton btnNext(BTN_NEXT_PIN);
ezButton btnLed(BTN_LED_PIN);

HardwareSerial DFPlayerSerial(2); // UART2 nativo del ESP32
DFRobotDFPlayerMini myDFPlayer;

// Villancico en turno: índice 0-based. La pista del DFPlayer es siempre
// currentVillancico + 1 (0001.mp3, 0002.mp3, 0003.mp3).
uint8_t currentVillancico = 0;

// true mientras la pista + la secuencia de LEDs del villancico en turno están activas.
bool isPlaying = false;

// ---------------------------------------------------------------
// IDENTIFICACIÓN DE CLIENTES POR ÍNDICE FIJO
// ---------------------------------------------------------------
// Cada ESP32-C3 manda, justo al conectarse, la línea "ID:n" (n = 0 a
// MAX_CLIENTES-1). Mientras no se identifique, queda en zona de espera.

WiFiClient pendientes[MAX_CLIENTES];
unsigned long pendienteInicio[MAX_CLIENTES];
const unsigned long TIMEOUT_IDENTIFICACION_MS = 5000;

void aceptarConexionNueva() {
  WiFiClient nuevoCliente = servidor.available();
  if (!nuevoCliente) return;

  for (int i = 0; i < MAX_CLIENTES; i++) {
    if (!pendientes[i] || !pendientes[i].connected()) {
      pendientes[i] = nuevoCliente;
      pendientes[i].setNoDelay(true);
      pendienteInicio[i] = millis();
      Serial.print(">>> Nueva conexión en espera de identificación (slot temporal ");
      Serial.print(i);
      Serial.println(")");
      return;
    }
  }

  Serial.println(">>> Conexión rechazada: no hay slots de espera libres");
  nuevoCliente.stop();
}

void procesarIdentificaciones() {
  for (int i = 0; i < MAX_CLIENTES; i++) {
    if (!pendientes[i]) continue;

    if (!pendientes[i].connected()) {
      pendientes[i].stop();
      continue;
    }

    if (pendientes[i].available()) {
      String linea = pendientes[i].readStringUntil('\n');
      linea.trim();

      if (linea.startsWith("ID:")) {
        int idx = linea.substring(3).toInt();
        if (idx >= 0 && idx < MAX_CLIENTES) {
          // Si ya había alguien en ese índice (reconexión), lo cerramos
          if (clientes[idx] && clientes[idx].connected()) {
            clientes[idx].stop();
          }
          clientes[idx] = pendientes[i];
          clientes[idx].setTimeout(100);  // límite duro: nunca esperar más de 100ms en este socket
          pendientes[i] = WiFiClient();  // libera el slot de espera
          Serial.print(">>> Cliente identificado como índice ");
          Serial.println(idx);
        } else {
          Serial.print(">>> ID inválido recibido: ");
          Serial.println(linea);
          pendientes[i].stop();
        }
      } else {
        Serial.print(">>> Línea inesperada (se esperaba ID:n): ");
        Serial.println(linea);
      }
      continue;
    }

    // Si tarda demasiado en identificarse, se descarta la conexión
    if (millis() - pendienteInicio[i] > TIMEOUT_IDENTIFICACION_MS) {
      Serial.print(">>> Timeout de identificación en slot temporal ");
      Serial.println(i);
      pendientes[i].stop();
    }
  }
}

// ---------------------------------------------------------------
// LED DE LATENCIA (MAESTRO)
// ---------------------------------------------------------------

// ESP32 clásico: usar literal 2, no LED_BUILTIN
const int PIN_LED_MAESTRO = 2;

bool masterLedIsOn = false;
unsigned long masterLedStartTime = 0;
unsigned long masterLedDuration = 0;

// Enciende el LED del maestro por la mitad de la duración indicada (no bloqueante).
void encenderLedMaestro(unsigned long duracionClienteMs) {
  digitalWrite(PIN_LED_MAESTRO, HIGH);
  masterLedIsOn = true;
  masterLedStartTime = millis();
  masterLedDuration = duracionClienteMs / 2;
}

// Revisa en cada vuelta del loop si ya toca apagar el LED del maestro.
void actualizarLedMaestro() {
  if (masterLedIsOn && (millis() - masterLedStartTime >= masterLedDuration)) {
    digitalWrite(PIN_LED_MAESTRO, LOW);
    masterLedIsOn = false;
  }
}

// ---------------------------------------------------------------
// SECUENCIA
// ---------------------------------------------------------------

// Valor centinela para un paso de silencio (MAX_CLIENTES es 8, 255 nunca colisiona).
#define SILENCE 255

#define BPM 60
// BPM define la duración de la NEGRA (1/4). Una nota entera = 4 negras.
#define MS_PER_WHOLE_NOTE (240000.0 / BPM)  // a 60bpm, 1/4 (negra) = 1000ms

// Hueco non-legato fijo: una fusa (1/32 de nota entera).
#define NON_LEGATO_GAP_FRACTION (1.0 / 32.0)
#define NON_LEGATO_GAP_MS ((unsigned long)(NON_LEGATO_GAP_FRACTION * MS_PER_WHOLE_NOTE))

// Formato de entrada: {clientNumber, numerador, denominador}
struct StepFraction {
  uint8_t clientNumber;
  uint8_t numerator;
  uint8_t denominator;
};
StepFraction nocheDePaz[] = {
  {4, 3, 8}, // 7 -> 4, 0.375 -> 3/8
  {5, 1, 8}, // 9 -> 5, 0.125 -> 1/8
  {4, 1, 4}, // 7 -> 4, 0.25 -> 1/4
  {2, 2, 4}, // 4 -> 2, 0.5 -> 2/4
  {SILENCE, 1, 4}, // SILENCE -> SILENCE, 0.25 -> 1/4
  {4, 3, 8},
  {5, 1, 8},
  {4, 1, 4},
  {2, 2, 4},
  {SILENCE, 1, 4},
  {1, 2, 4}, // 2 -> 1, 0.5 -> 2/4
  {1, 1, 4}, // 2 -> 1, 0.25 -> 1/4
  {6, 2, 4}, // 11 -> 6, 0.5 -> 2/4
  {6, 1, 4}, // 11 -> 6, 0.25 -> 1/4
  {7, 2, 4}, // 12 -> 7, 0.5 -> 2/4
  {7, 1, 4}, // 12 -> 7, 0.25 -> 1/4
  {4, 3, 4}, // 7 -> 4, 0.75 -> 3/4
  {5, 2, 4}, // 9 -> 5, 0.5 -> 2/4
  {5, 1, 4}, // 9 -> 5, 0.25 -> 1/4
  {7, 3, 8}, // 12 -> 7, 0.375 -> 3/8
  {6, 1, 8}, // 11 -> 6, 0.125 -> 1/8
  {5, 1, 4}, // 9 -> 5, 0.25 -> 1/4
  {4, 3, 8}, // 7 -> 4, 0.375 -> 3/8
  {5, 1, 8}, // 9 -> 5, 0.125 -> 1/8
  {4, 1, 4}, // 7 -> 4, 0.25 -> 1/4
  {2, 2, 4}, // 4 -> 2, 0.5 -> 2/4
  {SILENCE, 1, 4},
  {5, 2, 4}, // 9 -> 5, 0.5 -> 2/4
  {5, 1, 4}, // 9 -> 5, 0.25 -> 1/4
  {7, 3, 8}, // 12 -> 7, 0.375 -> 3/8
  {6, 1, 8}, // 11 -> 6, 0.125 -> 1/8
  {5, 1, 4}, // 9 -> 5, 0.25 -> 1/4
  {4, 3, 8}, // 7 -> 4, 0.375 -> 3/8
  {5, 1, 8}, // 9 -> 5, 0.125 -> 1/8
  {4, 1, 4}, // 7 -> 4, 0.25 -> 1/4
  {2, 2, 4}, // 4 -> 2, 0.5 -> 2/4
  {SILENCE, 1, 4},
  {1, 2, 4}, // 2 -> 1, 0.5 -> 2/4
  {1, 1, 4}, // 2 -> 1, 0.25 -> 1/4
  {3, 3, 8}, // 5 -> 3, 0.375 -> 3/8
  {1, 1, 8}, // 2 -> 1, 0.125 -> 1/8
  {6, 1, 4}, // 11 -> 6, 0.25 -> 1/4
  {0, 3, 4}, // 0 -> 0, 0.75 -> 3/4
  {2, 2, 4}, // 4 -> 2, 0.5 -> 2/4
  {7, 3, 8}, // 12 -> 7, 0.375 -> 3/8
  {4, 1, 8}, // 7 -> 4, 0.125 -> 1/8
  {2, 1, 4}, // 4 -> 2, 0.25 -> 1/4
  {4, 3, 8}, // 7 -> 4, 0.375 -> 3/8
  {3, 1, 8}, // 5 -> 3, 0.125 -> 1/8
  {1, 1, 4}, // 2 -> 1, 0.25 -> 1/4
  {0, 3, 4}, // 0 -> 0, 0.75 -> 3/4
  {SILENCE, 3, 4} // SILENCE -> SILENCE, 0.75 -> 3/4
};

StepFraction ninoDelTambor[] = {
  {0, 3, 4},       // 0 -> 0, 0.75 -> 3/4
  {1, 1, 4},       // 2 -> 1, 0.25 -> 1/4
  {2, 2, 4},       // 4 -> 2, 0.5 -> 2/4
  {2, 1, 4},       // 4 -> 2, 0.25 -> 1/4
  {2, 1, 4},       // 4 -> 2, 0.25 -> 1/4
  {3, 1, 8},       // 5 -> 3, 0.125 -> 1/8
  {2, 1, 8},       // 4 -> 2, 0.125 -> 1/8
  {3, 1, 4},       // 5 -> 3, 0.25 -> 1/4
  {2, 5, 4},       // 4 -> 2, 1.25 -> 5/4
  {SILENCE, 1, 4}, // SILENCE -> SILENCE, 0.25 -> 1/4
  {SILENCE, 1, 4}, // SILENCE -> SILENCE, 0.25 -> 1/4
  {0, 1, 4},       // 0 -> 0, 0.25 -> 1/4
  {0, 1, 4},       // 0 -> 0, 0.25 -> 1/4
  {1, 1, 4},       // 2 -> 1, 0.25 -> 1/4
  {2, 1, 4},       // 4 -> 2, 0.25 -> 1/4
  {2, 1, 4},       // 4 -> 2, 0.25 -> 1/4
  {2, 1, 4},       // 4 -> 2, 0.25 -> 1/4
  {2, 1, 4},       // 4 -> 2, 0.25 -> 1/4
  {3, 1, 8},       // 5 -> 3, 0.125 -> 1/8
  {2, 1, 8},       // 4 -> 2, 0.125 -> 1/8
  {3, 1, 4},       // 5 -> 3, 0.25 -> 1/4
  {2, 5, 4},       // 4 -> 2, 1.25 -> 5/4
  {SILENCE, 1, 4}, // SILENCE -> SILENCE, 0.25 -> 1/4
  {SILENCE, 1, 4}, // SILENCE -> SILENCE, 0.25 -> 1/4
  {1, 1, 4},       // 2 -> 1, 0.25 -> 1/4
  {2, 1, 4},       // 4 -> 2, 0.25 -> 1/4
  {3, 1, 4},       // 5 -> 3, 0.25 -> 1/4
  {4, 1, 4},       // 7 -> 4, 0.25 -> 1/4
  {4, 1, 4},       // 7 -> 4, 0.25 -> 1/4
  {4, 1, 4},       // 7 -> 4, 0.25 -> 1/4
  {5, 1, 4},       // 9 -> 5, 0.25 -> 1/4
  {4, 1, 8},       // 7 -> 4, 0.125 -> 1/8
  {3, 1, 8},       // 5 -> 3, 0.125 -> 1/8
  {2, 1, 4},       // 4 -> 2, 0.25 -> 1/4
  {1, 5, 4},       // 2 -> 1, 1.25 -> 5/4
  {SILENCE, 1, 4}, // SILENCE -> SILENCE, 0.25 -> 1/4
  {SILENCE, 1, 4}, // SILENCE -> SILENCE, 0.25 -> 1/4
  {1, 1, 4},       // 2 -> 1, 0.25 -> 1/4
  {2, 1, 4},       // 4 -> 2, 0.25 -> 1/4
  {3, 1, 4},       // 5 -> 3, 0.25 -> 1/4
  {4, 1, 4},       // 7 -> 4, 0.25 -> 1/4
  {4, 1, 4},       // 7 -> 4, 0.25 -> 1/4
  {4, 1, 4},       // 7 -> 4, 0.25 -> 1/4
  {5, 1, 4},       // 9 -> 5, 0.25 -> 1/4
  {4, 1, 8},       // 7 -> 4, 0.125 -> 1/8
  {5, 1, 8},       // 9 -> 5, 0.125 -> 1/8
  {4, 1, 4},       // 7 -> 4, 0.25 -> 1/4
  {3, 2, 4},       // 5 -> 3, 0.5 -> 2/4
  {5, 1, 8},       // 9 -> 5, 0.125 -> 1/8
  {4, 1, 8},       // 7 -> 4, 0.125 -> 1/8
  {3, 1, 4},       // 5 -> 3, 0.25 -> 1/4
  {2, 2, 4},       // 4 -> 2, 0.5 -> 2/4
  {4, 1, 8},       // 7 -> 4, 0.125 -> 1/8
  {3, 1, 8},       // 5 -> 3, 0.125 -> 1/8
  {2, 1, 4},       // 4 -> 2, 0.25 -> 1/4
  {1, 5, 4},       // 2 -> 1, 1.25 -> 5/4
  {SILENCE, 1, 4}, // SILENCE -> SILENCE, 0.25 -> 1/4
  {SILENCE, 1, 4}, // SILENCE -> SILENCE, 0.25 -> 1/4
  {0, 3, 4},       // 0 -> 0, 0.75 -> 3/4
  {1, 3, 4},       // 2 -> 1, 0.75 -> 3/4
  {2, 1, 4},       // 4 -> 2, 0.25 -> 1/4
  {2, 2, 4},       // 4 -> 2, 0.5 -> 2/4
  {2, 1, 4},       // 4 -> 2, 0.25 -> 1/4
  {3, 1, 4},       // 5 -> 3, 0.25 -> 1/4
  {2, 1, 8},       // 4 -> 2, 0.125 -> 1/8
  {3, 1, 8},       // 5 -> 3, 0.125 -> 1/8
  {2, 1, 4},       // 4 -> 2, 0.25 -> 1/4
  {2, 5, 4},       // 4 -> 2, 1.25 -> 5/4
  {SILENCE, 1, 4}, // SILENCE -> SILENCE, 0.25 -> 1/4
  {1, 1, 8},       // 2 -> 1, 0.125 -> 1/8
  {0, 1, 8},       // 0 -> 0, 0.125 -> 1/8
  {1, 1, 4},       // 2 -> 1, 0.25 -> 1/4
  {0, 5, 4},       // 0 -> 0, 1.25 -> 5/4
  {SILENCE, 1, 4}  // SILENCE -> SILENCE, 0.25 -> 1/4
};

StepFraction joyToTheWorld[] = {
  {SILENCE, 17, 4}, // SILENCE -> SILENCE, 4.25 -> 17/4
  {7, 1, 4},        // 12 -> 7, 0.25 -> 1/4
  {6, 3, 16},       // 11 -> 6, 0.1875 -> 3/16
  {5, 1, 16},       // 9 -> 5, 0.0625 -> 1/16
  {4, 3, 8},        // 7 -> 4, 0.375 -> 3/8
  {3, 1, 8},        // 5 -> 3, 0.125 -> 1/8
  {2, 1, 4},        // 4 -> 2, 0.25 -> 1/4
  {1, 1, 4},        // 2 -> 1, 0.25 -> 1/4
  {0, 3, 8},        // 0 -> 0, 0.375 -> 3/8
  {4, 1, 8},        // 7 -> 4, 0.125 -> 1/8
  {5, 3, 8},        // 9 -> 5, 0.375 -> 3/8
  {5, 1, 8},        // 9 -> 5, 0.125 -> 1/8
  {6, 3, 8},        // 11 -> 6, 0.375 -> 3/8
  {6, 1, 8},        // 11 -> 6, 0.125 -> 1/8
  {7, 3, 4},        // 12 -> 7, 0.75 -> 3/4
  {SILENCE, 1, 8},  // SILENCE -> SILENCE, 0.125 -> 1/8
  {7, 1, 8},        // 12 -> 7, 0.125 -> 1/8
  {7, 1, 8},        // 12 -> 7, 0.125 -> 1/8
  {6, 1, 8},        // 11 -> 6, 0.125 -> 1/8
  {5, 1, 8},        // 9 -> 5, 0.125 -> 1/8
  {4, 1, 8},        // 7 -> 4, 0.125 -> 1/8
  {4, 3, 16},       // 7 -> 4, 0.1875 -> 3/16
  {3, 1, 16},       // 5 -> 3, 0.0625 -> 1/16
  {2, 1, 8},        // 4 -> 2, 0.125 -> 1/8
  {7, 1, 8},        // 12 -> 7, 0.125 -> 1/8
  {7, 1, 8},        // 12 -> 7, 0.125 -> 1/8
  {6, 1, 8},        // 11 -> 6, 0.125 -> 1/8
  {5, 1, 8},        // 9 -> 5, 0.125 -> 1/8
  {4, 1, 8},        // 7 -> 4, 0.125 -> 1/8
  {4, 3, 16},       // 7 -> 4, 0.1875 -> 3/16
  {3, 1, 16},       // 5 -> 3, 0.0625 -> 1/16
  {2, 1, 8},        // 4 -> 2, 0.125 -> 1/8
  {2, 1, 8},        // 4 -> 2, 0.125 -> 1/8
  {2, 1, 8},        // 4 -> 2, 0.125 -> 1/8
  {2, 1, 8},        // 4 -> 2, 0.125 -> 1/8
  {2, 1, 8},        // 4 -> 2, 0.125 -> 1/8
  {2, 1, 16},       // 4 -> 2, 0.0625 -> 1/16
  {3, 1, 16},       // 5 -> 3, 0.0625 -> 1/16
  {4, 3, 8},        // 7 -> 4, 0.375 -> 3/8
  {3, 1, 16},       // 5 -> 3, 0.0625 -> 1/16
  {2, 1, 16},       // 4 -> 2, 0.0625 -> 1/16
  {1, 1, 8},        // 2 -> 1, 0.125 -> 1/8
  {1, 1, 8},        // 2 -> 1, 0.125 -> 1/8
  {1, 1, 8},        // 2 -> 1, 0.125 -> 1/8
  {1, 1, 16},       // 2 -> 1, 0.0625 -> 1/16
  {2, 1, 16},       // 4 -> 2, 0.0625 -> 1/16
  {3, 3, 8},        // 5 -> 3, 0.375 -> 3/8
  {2, 1, 16},       // 4 -> 2, 0.0625 -> 1/16
  {1, 1, 16},       // 2 -> 1, 0.0625 -> 1/16
  {0, 1, 8},        // 0 -> 0, 0.125 -> 1/8
  {7, 1, 4},        // 12 -> 7, 0.25 -> 1/4
  {5, 1, 8},        // 9 -> 5, 0.125 -> 1/8
  {4, 3, 16},       // 7 -> 4, 0.1875 -> 3/16
  {3, 1, 16},       // 5 -> 3, 0.0625 -> 1/16
  {2, 1, 8},        // 4 -> 2, 0.125 -> 1/8
  {3, 1, 8},        // 5 -> 3, 0.125 -> 1/8
  {2, 1, 4},        // 4 -> 2, 0.25 -> 1/4
  {1, 1, 4},        // 2 -> 1, 0.25 -> 1/4
  {0, 7, 16},       // 0 -> 0, 0.4375 -> 7/16
  {SILENCE, 1, 16}, // SILENCE -> SILENCE, 0.0625 -> 1/16
  {7, 1, 4},        // 12 -> 7, 0.25 -> 1/4
  {6, 3, 16},       // 11 -> 6, 0.1875 -> 3/16
  {5, 1, 16},       // 9 -> 5, 0.0625 -> 1/16
  {4, 3, 8},        // 7 -> 4, 0.375 -> 3/8
  {3, 1, 8},        // 5 -> 3, 0.125 -> 1/8
  {2, 1, 4},        // 4 -> 2, 0.25 -> 1/4
  {1, 1, 4},        // 2 -> 1, 0.25 -> 1/4
  {0, 3, 8},        // 0 -> 0, 0.375 -> 3/8
  {4, 1, 8},        // 7 -> 4, 0.125 -> 1/8
  {5, 3, 8},        // 9 -> 5, 0.375 -> 3/8
  {5, 1, 8},        // 9 -> 5, 0.125 -> 1/8
  {6, 3, 8},        // 11 -> 6, 0.375 -> 3/8
  {6, 1, 8},        // 11 -> 6, 0.125 -> 1/8
  {7, 3, 4},        // 12 -> 7, 0.75 -> 3/4
  {SILENCE, 1, 8},  // SILENCE -> SILENCE, 0.125 -> 1/8
  {7, 1, 8},        // 12 -> 7, 0.125 -> 1/8
  {7, 1, 8},        // 12 -> 7, 0.125 -> 1/8
  {6, 1, 8},        // 11 -> 6, 0.125 -> 1/8
  {5, 1, 8},        // 9 -> 5, 0.125 -> 1/8
  {4, 1, 8},        // 7 -> 4, 0.125 -> 1/8
  {4, 3, 16},       // 7 -> 4, 0.1875 -> 3/16
  {3, 1, 16},       // 5 -> 3, 0.0625 -> 1/16
  {2, 1, 8},        // 4 -> 2, 0.125 -> 1/8
  {7, 1, 8},        // 12 -> 7, 0.125 -> 1/8
  {7, 1, 8},        // 12 -> 7, 0.125 -> 1/8
  {6, 1, 8},        // 11 -> 6, 0.125 -> 1/8
  {5, 1, 8},        // 9 -> 5, 0.125 -> 1/8
  {4, 1, 8},        // 7 -> 4, 0.125 -> 1/8
  {4, 3, 16},       // 7 -> 4, 0.1875 -> 3/16
  {3, 1, 16},       // 5 -> 3, 0.0625 -> 1/16
  {2, 1, 8},        // 4 -> 2, 0.125 -> 1/8
  {2, 1, 8},        // 4 -> 2, 0.125 -> 1/8
  {2, 1, 8},        // 4 -> 2, 0.125 -> 1/8
  {2, 1, 8},        // 4 -> 2, 0.125 -> 1/8
  {2, 1, 8},        // 4 -> 2, 0.125 -> 1/8
  {2, 1, 16},       // 4 -> 2, 0.0625 -> 1/16
  {3, 1, 16},       // 5 -> 3, 0.0625 -> 1/16
  {4, 3, 8},        // 7 -> 4, 0.375 -> 3/8
  {3, 1, 16},       // 5 -> 3, 0.0625 -> 1/16
  {2, 1, 16},       // 4 -> 2, 0.0625 -> 1/16
  {1, 1, 8},        // 2 -> 1, 0.125 -> 1/8
  {1, 1, 8},        // 2 -> 1, 0.125 -> 1/8
  {1, 1, 8},        // 2 -> 1, 0.125 -> 1/8
  {1, 1, 16},       // 2 -> 1, 0.0625 -> 1/16
  {2, 1, 16},       // 4 -> 2, 0.0625 -> 1/16
  {3, 3, 8},        // 5 -> 3, 0.375 -> 3/8
  {2, 1, 16},       // 4 -> 2, 0.0625 -> 1/16
  {1, 1, 16},       // 2 -> 1, 0.0625 -> 1/16
  {0, 1, 8},        // 0 -> 0, 0.125 -> 1/8
  {7, 1, 4},        // 12 -> 7, 0.25 -> 1/4
  {5, 1, 8},        // 9 -> 5, 0.125 -> 1/8
  {4, 3, 16},       // 7 -> 4, 0.1875 -> 3/16
  {3, 1, 16},       // 5 -> 3, 0.0625 -> 1/16
  {2, 1, 8},        // 4 -> 2, 0.125 -> 1/8
  {3, 1, 8},        // 5 -> 3, 0.125 -> 1/8
  {2, 1, 4},        // 4 -> 2, 0.25 -> 1/4
  {1, 1, 4},        // 2 -> 1, 0.25 -> 1/4
  {0, 7, 16},       // 0 -> 0, 0.4375 -> 7/16
  {SILENCE, 1, 16}, // SILENCE -> SILENCE, 0.0625 -> 1/16
  {7, 1, 8},        // 12 -> 7, 0.125 -> 1/8
  {6, 1, 8},        // 11 -> 6, 0.125 -> 1/8
  {5, 1, 8},        // 9 -> 5, 0.125 -> 1/8
  {4, 1, 8},        // 7 -> 4, 0.125 -> 1/8
  {3, 1, 8},        // 5 -> 3, 0.125 -> 1/8
  {2, 1, 8},        // 4 -> 2, 0.125 -> 1/8
  {1, 1, 8},        // 2 -> 1, 0.125 -> 1/8
  {7, 1, 8},        // 12 -> 7, 0.125 -> 1/8
  {6, 1, 8},        // 11 -> 6, 0.125 -> 1/8
  {5, 1, 8},        // 9 -> 5, 0.125 -> 1/8
  {4, 1, 8},        // 7 -> 4, 0.125 -> 1/8
  {3, 1, 8},        // 5 -> 3, 0.125 -> 1/8
  {2, 1, 8},        // 4 -> 2, 0.125 -> 1/8
  {1, 1, 8},        // 2 -> 1, 0.125 -> 1/8
  {0, 1, 8},        // 0 -> 0, 0.125 -> 1/8
  {0, 17, 16}       // 0 -> 0, 1.0625 -> 17/16
};

// ---------------------------------------------------------------
// SELECCIÓN DE VILLANCICOS
// ---------------------------------------------------------------
// Mismo orden que las pistas del DFPlayer (índice 0 = 0001.mp3, etc.).
StepFraction* villancicos[] = { nocheDePaz, ninoDelTambor, joyToTheWorld };
const uint16_t villancicoLengths[] = {
  sizeof(nocheDePaz) / sizeof(nocheDePaz[0]),
  sizeof(ninoDelTambor) / sizeof(ninoDelTambor[0]),
  sizeof(joyToTheWorld) / sizeof(joyToTheWorld[0])
};

// Nombres que se muestran en el LCD (máx. 16 caracteres, sin acentos/ñ
// porque el LCD1602 no los tiene en su ROM de caracteres).
const char* nombres[] = { "Noche de Paz", "Nino del Tambor", "Joy to the World" };
const uint8_t NUM_VILLANCICOS = sizeof(villancicos) / sizeof(villancicos[0]);

struct Step {
  uint8_t clientNumber;
  unsigned long durationMs;
};

Step stepsNocheDePaz[sizeof(nocheDePaz) / sizeof(nocheDePaz[0])];
Step stepsNinoDelTambor[sizeof(ninoDelTambor) / sizeof(ninoDelTambor[0])];
Step stepsJoyToTheWorld[sizeof(joyToTheWorld) / sizeof(joyToTheWorld[0])];

Step* stepsPorVillancico[] = { stepsNocheDePaz, stepsNinoDelTambor, stepsJoyToTheWorld };

void convertirSecuencia() {
  for (uint8_t v = 0; v < NUM_VILLANCICOS; v++) {
    StepFraction* origen = villancicos[v];
    Step* destino = stepsPorVillancico[v];
    uint16_t largo = villancicoLengths[v];
    for (uint16_t i = 0; i < largo; i++) {
      destino[i].clientNumber = origen[i].clientNumber;
      float fraccion = (float)origen[i].numerator / (float)origen[i].denominator;
      destino[i].durationMs = (unsigned long)(fraccion * MS_PER_WHOLE_NOTE);
    }
  }
}

uint16_t currentStep = 0;
unsigned long stepStartTime = 0;
bool stepEnCurso = false;
bool notaSeparada = false;       // true una vez enviado el OFF anticipado (non-legato)
bool secuenciaTerminada = false; // true cuando el villancico en turno ya tocó todos sus pasos

// Cliente cuyo LED está encendido por la secuencia (-1 = ninguno).
int8_t clienteEncendidoActual = -1;

// ---------------------------------------------------------------
// LCD: pantalla de estado
// ---------------------------------------------------------------

// Escribe un texto en una fila rellenando con espacios hasta 16 columnas,
// para borrar restos del texto anterior sin usar lcd.clear() (que parpadea).
void lcdImprimirFila(uint8_t fila, const char* texto) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", texto);
  lcd.setCursor(0, fila);
  lcd.print(buf);
}

// Muestra solo el nombre del villancico seleccionado (sin importar si está
// en Play o Stop). Llamar al arrancar y al cambiar con Siguiente/Anterior.
void actualizarLCD() {
  lcdImprimirFila(0, nombres[currentVillancico]);
  lcdImprimirFila(1, "");
}

// ---------------------------------------------------------------

void enviarComando(uint8_t clientIndex, const char* comando) {
  if (clientIndex >= MAX_CLIENTES) return;  // cubre también SILENCE (255)
  if (clientes[clientIndex] && clientes[clientIndex].connected()) {
    // println() puede bloquearse si el socket de este cliente se atasca.
    // (Verificación de availableForWrite desactivada a propósito.)
//    size_t bytesNecesarios = strlen(comando) + 2; // +2 por el "\r\n" de println()
//    if ((size_t)clientes[clientIndex].availableForWrite() < bytesNecesarios) {
//      Serial.print(">>> Comando descartado (buffer lleno) para cliente ");
//      Serial.println(clientIndex);
//      return;
//    }
    clientes[clientIndex].println(comando);
    Serial.print("[Enviado: ");
    Serial.print(comando);
    Serial.print("] a cliente ");
    Serial.println(clientIndex);
  }
}

// Reinicia la secuencia de LEDs del villancico en turno desde su primer paso.
void iniciarSecuenciaDesdeElInicio() {
  currentStep = 0;
  stepEnCurso = false;
  notaSeparada = false;
  secuenciaTerminada = false;
}

// Apaga de inmediato cualquier LED encendido por la secuencia (cliente en
// turno + LED de latencia del maestro).
void detenerLEDsInmediatamente() {
  if (clienteEncendidoActual != -1) {
    enviarComando((uint8_t)clienteEncendidoActual, "OFF");
    clienteEncendidoActual = -1;
  }
  digitalWrite(PIN_LED_MAESTRO, LOW);
  masterLedIsOn = false;
}

void actualizarSecuencia() {
  if (!isPlaying || secuenciaTerminada) return;

  Step* secuenciaActual = stepsPorVillancico[currentVillancico];
  uint16_t largoActual = villancicoLengths[currentVillancico];

  unsigned long ahora = millis();
  bool esSilencio = (secuenciaActual[currentStep].clientNumber == SILENCE);
  unsigned long duracionTotal = secuenciaActual[currentStep].durationMs;

  if (!stepEnCurso) {
    // Inicia el paso actual.
    if (esSilencio) {
      Serial.println("[Silencio]");
    } else {
      enviarComando(secuenciaActual[currentStep].clientNumber, "ON");
      clienteEncendidoActual = secuenciaActual[currentStep].clientNumber;
      encenderLedMaestro(duracionTotal);
    }

    stepEnCurso = true;
    notaSeparada = false;
    stepStartTime = ahora;
    return;
  }

  unsigned long transcurrido = ahora - stepStartTime;
  unsigned long duracionSonando = (duracionTotal > NON_LEGATO_GAP_MS) ? (duracionTotal - NON_LEGATO_GAP_MS) : 0;

  // Corta el LED NON_LEGATO_GAP_MS antes del final del paso.
  if (!esSilencio && !notaSeparada && transcurrido >= duracionSonando) {
    enviarComando(secuenciaActual[currentStep].clientNumber, "OFF");
    clienteEncendidoActual = -1;
    notaSeparada = true;
  }

  // Ya se cumplió la duración total del paso: avanza al siguiente
  if (transcurrido >= duracionTotal) {
    if (!esSilencio && !notaSeparada) {
      enviarComando(secuenciaActual[currentStep].clientNumber, "OFF");
      clienteEncendidoActual = -1;
    }
    stepEnCurso = false;

    if (currentStep + 1 >= largoActual) {
      // El villancico terminó solo: queda esperando Play/Siguiente/Anterior.
      secuenciaTerminada = true;
      isPlaying = false;
      Serial.println(">>> Villancico terminado. Presiona Play o Siguiente/Anterior.");
    } else {
      currentStep++;
    }
  }
}

// ---------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_LED_MAESTRO, OUTPUT);
  digitalWrite(PIN_LED_MAESTRO, LOW);

  convertirSecuencia();

  // --- LCD (I2C por defecto del ESP32: SDA=21, SCL=22) ---
  Wire.begin();
  lcd.init();
  lcd.backlight();

  // --- Botones (antirrebote 50ms) ---
  btnReset.setDebounceTime(50);
  btnPlay.setDebounceTime(50);
  btnPrev.setDebounceTime(50);
  btnNext.setDebounceTime(50);
  btnLed.setDebounceTime(30);

  // --- DFPlayer Mini por UART2 ---
  DFPlayerSerial.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);
  Serial.println(F("Inicializando DFPlayer Mini..."));
  if (!myDFPlayer.begin(DFPlayerSerial)) {
    Serial.println(F("Error: Comprueba las conexiones o la tarjeta SD."));
    while (true); // Detener si hay fallo
  }
  myDFPlayer.volume(20); // Volumen de 0 a 30
  Serial.println(F("DFPlayer Mini listo."));

  WiFi.mode(WIFI_STA);
  WiFi.config(ipFija, gateway, subnet);
  WiFi.begin(ssid, password);

  Serial.print("Conectando a ");
  Serial.println(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Desactiva el power-save del WiFi para reducir latencia.
  WiFi.setSleep(false);

  Serial.println();
  Serial.println("=================================");
  Serial.println("Conectado al router exitosamente");
  Serial.print("IP del ESP32 (maestro): ");
  Serial.println(WiFi.localIP());
  Serial.println("=================================");

  servidor.begin();
  Serial.println("Servidor iniciado. Esperando ESP32-C3...");

  // Pantalla inicial: nombre del villancico en turno
  actualizarLCD();
}

void loop() {
  // 1. Aceptar nuevas conexiones (van a la zona de espera, sin índice aún)
  aceptarConexionNueva();

  // 2. Revisar si algún pendiente ya mandó su "ID:n" y asignarlo a su slot fijo
  procesarIdentificaciones();

  // 3. Leer los 4 botones físicos
  btnReset.loop();
  btnPlay.loop();
  btnPrev.loop();
  btnNext.loop();
  btnLed.loop();

  // STOP (GPIO 27): pausa la pista y apaga los LEDs ya
  if (btnReset.isPressed()) {
    Serial.println(F("Accion: Stop"));
    myDFPlayer.pause();
    isPlaying = false;
    detenerLEDsInmediatamente();
  }

  // PLAY (GPIO 12): (re)inicia pista + secuencia del villancico en turno
  if (btnPlay.isPressed()) {
    Serial.print(F("Accion: Play - "));
    Serial.println(nombres[currentVillancico]);
    detenerLEDsInmediatamente();  // por si se presiona Play con un LED encendido
    myDFPlayer.playMp3Folder(currentVillancico + 1);
    isPlaying = true;
    iniciarSecuenciaDesdeElInicio();
  }

  // ANTERIOR (GPIO 14): villancico anterior (circular)
  if (btnPrev.isPressed()) {
    detenerLEDsInmediatamente();

    currentVillancico = (currentVillancico == 0) ? (NUM_VILLANCICOS - 1) : (currentVillancico - 1);
    Serial.print(F("Villancico seleccionado: "));
    Serial.println(nombres[currentVillancico]);

    if (isPlaying) {
      Serial.println(F("-> Reproduciendo villancico anterior..."));
      myDFPlayer.playMp3Folder(currentVillancico + 1);
      iniciarSecuenciaDesdeElInicio();
    } else {
      secuenciaTerminada = false;  // el nuevo villancico aún no ha tocado nada
      Serial.println(F("-> Cambiado en silencio. Presiona PLAY."));
    }
    actualizarLCD();
  }

  // SIGUIENTE (GPIO 13): villancico siguiente (circular)
  if (btnNext.isPressed()) {
    detenerLEDsInmediatamente();

    currentVillancico = (currentVillancico + 1) % NUM_VILLANCICOS;
    Serial.print(F("Villancico seleccionado: "));
    Serial.println(nombres[currentVillancico]);

    if (isPlaying) {
      Serial.println(F("-> Reproduciendo villancico siguiente..."));
      myDFPlayer.playMp3Folder(currentVillancico + 1);
      iniciarSecuenciaDesdeElInicio();
    } else {
      secuenciaTerminada = false;
      Serial.println(F("-> Cambiado en silencio. Presiona PLAY."));
    }
    actualizarLCD();
  }

 // Botón directo: LED del cliente 4 mientras se mantenga presionado
  if (btnLed.isPressed()) {
    Serial.print("Botón Cliente 4 presionado");
    enviarComando(CLIENTE_BOTON, "ON");
  }
  if (btnLed.isReleased()) {
    // Si la secuencia está usando justo ese LED, no lo apagues desde aquí
    if (clienteEncendidoActual != CLIENTE_BOTON) {
      enviarComando(CLIENTE_BOTON, "OFF");
    }
  }

  // 4. Avanzar la secuencia de LEDs del villancico en turno (si está sonando)
  actualizarSecuencia();

  // 5. Apagar el LED de latencia del maestro cuando corresponda
  actualizarLedMaestro();
}
