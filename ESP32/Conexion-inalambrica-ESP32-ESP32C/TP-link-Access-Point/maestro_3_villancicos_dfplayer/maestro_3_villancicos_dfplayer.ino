/*
  ESP32 (DevKit clásico) - CLIENTE del router + CONTROLADOR DE LED REMOTO (multi-cliente)
                          + CONTROL DE DFPlayer Mini (audio) + 4 BOTONES FÍSICOS
  -------------------------------------------------------------------
  Se conecta al router TP-Link (en vez de crear su propia red) y manda
  comandos "ON" y "OFF" a los ESP32-C3 conectados, siguiendo una
  secuencia programable (array de pasos: cliente + duración en fracción de beat).

  Villancicos: "Noche de Paz" (0001.mp3), "El Niño del Tambor" (0002.mp3) y
  "Joy to the World" (0003.mp3). En todo momento hay un "villancico en turno"
  (currentVillancico, 0/1/2) que determina QUÉ pista suena en el DFPlayer y
  QUÉ arreglo de LEDs se está reproduciendo; ambos siempre van de la mano:
    - Al presionar PLAY: arranca la pista del villancico en turno en el
      DFPlayer y, en el mismo instante, la secuencia de LEDs de ese
      villancico reinicia desde su primer paso.
    - Al presionar STOP: se pausa la pista y se apaga de inmediato cualquier
      LED que siguiera encendido (sin esperar al hueco non-legato).
    - Al presionar SIGUIENTE/ANTERIOR: cambia el villancico en turno de forma
      circular. Si ya estaba sonando algo, apaga el LED en turno, cambia de
      pista en el DFPlayer y reinicia la secuencia de LEDs del nuevo
      villancico desde el paso 0. Si estaba detenido, solo cambia la
      selección en silencio (hay que presionar PLAY para que suene).
    - Si un villancico termina todos sus pasos por sí solo (sin presionar
      Stop), la secuencia se detiene y queda esperando Play/Siguiente/Anterior
      (no salta automáticamente al siguiente villancico).

  Mapeo de notas a índice de cliente:
    0 = do
    1 = re
    2 = mi
    3 = fa
    4 = sol
    5 = la
    6 = si
    7 = do (una octava arriba)
  SILENCE = paso de silencio: durante su duración, NINGÚN cliente
  recibe la señal de encendido (ni tampoco parpadea el LED del maestro).

  LED de latencia (maestro):
  Cada vez que se envía el comando "ON" a un cliente, el LED local del
  maestro (pin 2) se enciende al mismo tiempo, para poder ver a simple
  vista el retraso entre el encendido del maestro y el del cliente.
  El LED del maestro se apaga automáticamente tras la MITAD de la
  duración que se le mandó al cliente para ese paso (no espera a que
  el cliente reciba su "OFF"). En los pasos de SILENCE, el LED del
  maestro simplemente no se enciende.

  Separación non-legato entre notas:
  Cada nota (que no sea SILENCE) se apaga NON_LEGATO_GAP_MS antes de que
  termine su paso, dejando un pequeño hueco de silencio antes de la
  siguiente nota. Esa duración es fija (una fusa, 1/32 de nota entera) y
  no depende de la duración de la nota; el paso en sí sigue durando lo
  mismo, así que el tempo global no se altera.

  Botones físicos (antirrebote con ezButton):
    BTN_RESET_PIN (27) -> Stop:  pausa el DFPlayer y apaga los LEDs ya.
    BTN_PLAY_PIN  (12) -> Play:  (re)inicia pista + secuencia del villancico
                                  en turno, desde el principio.
    BTN_PREV_PIN  (14) -> Anterior: retrocede al villancico anterior
                                     (circular) y lo reproduce si ya sonaba.
    BTN_NEXT_PIN  (13) -> Siguiente: avanza al villancico siguiente
                                      (circular) y lo reproduce si ya sonaba.

  Cómo probarlo:
  1. Sube este sketch al ESP32 clásico (requiere las librerías ezButton y
     DFRobotDFPlayerMini).
  2. Sube el sketch cliente a cada ESP32-C3, apuntando al SSID del router
     y a la IP fija 192.168.0.150.
  3. Con la tarjeta SD del DFPlayer conteniendo 0001.mp3/0002.mp3/0003.mp3
     en la carpeta MP3, presiona Play: debería sonar "Noche de Paz" y los
     LEDs deberían encenderse uno por uno siguiendo su secuencia, respetando
     silencios y la separación non-legato. Prueba Stop, Siguiente y
     Anterior para confirmar que pista y LEDs cambian juntos.
*/

#include <WiFi.h>
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
// DFPLAYER MINI + BOTONES FÍSICOS
// ---------------------------------------------------------------

// Definición exacta de los pines de botones físicos:
#define BTN_RESET_PIN 27  // Stop
#define BTN_PLAY_PIN  12  // Play
#define BTN_PREV_PIN  14  // Retroceder al villancico anterior
#define BTN_NEXT_PIN  13  // Avanzar al villancico siguiente

// Pines para UART2 (DFPlayer)
#define RX2_PIN 16  // Conectar al TX del DFPlayer
#define TX2_PIN 17  // Conectar al RX del DFPlayer (con resistencia de 1k en serie)

ezButton btnReset(BTN_RESET_PIN);
ezButton btnPlay(BTN_PLAY_PIN);
ezButton btnPrev(BTN_PREV_PIN);
ezButton btnNext(BTN_NEXT_PIN);

HardwareSerial DFPlayerSerial(2); // UART2 nativo del ESP32
DFRobotDFPlayerMini myDFPlayer;

// Villancico en turno: índice 0-based (0=Noche de Paz, 1=Niño del Tambor,
// 2=Joy to the World). La pista del DFPlayer correspondiente es siempre
// currentVillancico + 1 (0001.mp3, 0002.mp3, 0003.mp3).
uint8_t currentVillancico = 0;

// true mientras la pista + la secuencia de LEDs del villancico en turno
// están activas (equivalente al "isPlaying" del sketch de audio original).
bool isPlaying = false;

// ---------------------------------------------------------------
// IDENTIFICACIÓN DE CLIENTES POR ÍNDICE FIJO
// ---------------------------------------------------------------
// Cada ESP32-C3 debe mandar, justo al conectarse, la línea "ID:n"
// (por ejemplo "ID:2") donde n es su índice fijo (0 a MAX_CLIENTES-1),
// definido en su propio sketch (una constante distinta por dispositivo
// antes de flashearlo). Mientras no se identifique, la conexión se
// mantiene en una zona de espera y NO se le manda la secuencia.

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

// ESP32 clásico: usar literal 2, no LED_BUILTIN (ver hardware-notes)
const int PIN_LED_MAESTRO = 2;

bool masterLedIsOn = false;
unsigned long masterLedStartTime = 0;
unsigned long masterLedDuration = 0;

// Enciende el LED del maestro por la mitad de la duración indicada
// para el paso actual (no bloqueante).
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

// Valor centinela para un paso de silencio: ningún cliente real usa
// este índice (MAX_CLIENTES es 8, así que 255 nunca colisiona).
#define SILENCE 255

#define BPM 60
// Notación musical estándar: BPM define la duración de la NEGRA (1/4).
// Una nota entera (1/1) equivale a 4 negras.
#define MS_PER_WHOLE_NOTE (240000.0 / BPM)  // a 60bpm, 1/4 (negra) = 1000ms

// Duración fija del "aire" non-legato entre notas: una fusa (1/32 de nota
// entera), independientemente de la duración de la nota que se esté tocando.
// El LED se apaga esa cantidad de tiempo antes de que termine el paso; el
// paso en sí sigue durando lo mismo, así que el tempo/timing global no cambia.
#define NON_LEGATO_GAP_FRACTION (1.0 / 32.0)
#define NON_LEGATO_GAP_MS ((unsigned long)(NON_LEGATO_GAP_FRACTION * MS_PER_WHOLE_NOTE))

// Formato de entrada: {clientNumber, numerador, denominador}
// clientNumber: 0 a 7 (índice en el array `clientes`, mapeado a la nota
// do-re-mi-fa-sol-la-si-do), o SILENCE para un silencio
// numerador/denominador: fracción de beat, ej. {1,4} = 1/4 de beat
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
// villancicos[]: arreglo de PUNTEROS a cada melodía definida arriba, en el
// mismo orden que las pistas del DFPlayer (índice 0 = 0001.mp3, etc.).
// Como los tres arreglos tienen distinto tamaño, villancicoLengths[] guarda
// cuántos pasos tiene el villancico correspondiente en el mismo índice
// (sizeof() de un puntero no sirve para saber el largo de lo que apunta).
StepFraction* villancicos[] = { nocheDePaz, ninoDelTambor, joyToTheWorld };
const uint16_t villancicoLengths[] = {
  sizeof(nocheDePaz) / sizeof(nocheDePaz[0]),
  sizeof(ninoDelTambor) / sizeof(ninoDelTambor[0]),
  sizeof(joyToTheWorld) / sizeof(joyToTheWorld[0])
};
const char* villancicoNombres[] = { "Noche de Paz", "El Nino del Tambor", "Joy to the World" };
const uint8_t NUM_VILLANCICOS = sizeof(villancicos) / sizeof(villancicos[0]);

// Cada villancico se precalcula (fracción -> milisegundos) en su propio
// arreglo Step, del tamaño exacto que le corresponde. A diferencia de la
// versión anterior, ya NO se concatenan: en cada momento solo se reproduce
// el villancico en turno (currentVillancico), sincronizado con su pista.
struct Step {
  uint8_t clientNumber;
  unsigned long durationMs;
};

Step stepsNocheDePaz[sizeof(nocheDePaz) / sizeof(nocheDePaz[0])];
Step stepsNinoDelTambor[sizeof(ninoDelTambor) / sizeof(ninoDelTambor[0])];
Step stepsJoyToTheWorld[sizeof(joyToTheWorld) / sizeof(joyToTheWorld[0])];

// Punteros a cada arreglo Step ya calculado, en el mismo orden/índice que
// villancicos[] y villancicoLengths[].
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
bool notaSeparada = false;       // true una vez que ya se mandó el OFF anticipado (non-legato)
bool secuenciaTerminada = false; // true cuando el villancico en turno ya tocó todos sus pasos

// Cliente cuyo LED está actualmente encendido por la secuencia (-1 = ninguno).
// Permite que Stop/Siguiente/Anterior apaguen el LED en turno de inmediato,
// sin esperar a que le toque su OFF por temporización normal.
int8_t clienteEncendidoActual = -1;

void enviarComando(uint8_t clientIndex, const char* comando) {
  if (clientIndex >= MAX_CLIENTES) return;  // cubre también SILENCE (255)
  if (clientes[clientIndex] && clientes[clientIndex].connected()) {
    // println() puede bloquearse si el socket de este cliente se queda
    // atascado (reporta connected()==true pero ya no acepta datos sin
    // esperar). Como loop() es de un solo hilo, ese bloqueo congelaría TODA
    // la secuencia de LEDs (aunque el DFPlayer, al ser hardware aparte,
    // seguiría sonando). Por eso primero se checa que el socket pueda
    // aceptar los bytes sin esperar; si no puede, se descarta este comando
    // puntual en vez de arriesgar que todo el villancico se congele.
    size_t bytesNecesarios = strlen(comando) + 2; // +2 por el "\r\n" de println()
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
// Se llama cada vez que se (re)empieza a reproducir una pista (Play, o
// Siguiente/Anterior mientras ya estaba sonando).
void iniciarSecuenciaDesdeElInicio() {
  currentStep = 0;
  stepEnCurso = false;
  notaSeparada = false;
  secuenciaTerminada = false;
}

// Apaga de inmediato cualquier LED que la secuencia tuviera encendido en
// este momento (cliente en turno + LED de latencia del maestro), sin
// esperar el hueco non-legato ni el fin natural del paso. Se usa en Stop y
// justo antes de cambiar de villancico con Siguiente/Anterior.
void detenerLEDsInmediatamente() {
  if (clienteEncendidoActual != -1) {
    enviarComando((uint8_t)clienteEncendidoActual, "OFF");
    clienteEncendidoActual = -1;
  }
  digitalWrite(PIN_LED_MAESTRO, LOW);
  masterLedIsOn = false;
}

void actualizarSecuencia() {
  // Detenido (Stop, o aún no se ha presionado Play) o el villancico en
  // turno ya tocó todos sus pasos: no hay nada que avanzar.
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
      // Enciende el LED del cliente correspondiente a la nota
      enviarComando(secuenciaActual[currentStep].clientNumber, "ON");
      clienteEncendidoActual = secuenciaActual[currentStep].clientNumber;
      // Enciende también el LED local del maestro (medio tiempo del cliente).
      // Durante un silencio no se enciende ningún LED, ni siquiera el del maestro.
      encenderLedMaestro(duracionTotal);
    }

    stepEnCurso = true;
    notaSeparada = false;
    stepStartTime = ahora;
    return;
  }

  unsigned long transcurrido = ahora - stepStartTime;
  // El LED se apaga NON_LEGATO_GAP_MS antes del final del paso, sin importar
  // cuánto dure la nota. Si la nota es más corta que ese hueco (no debería
  // pasar con las duraciones usadas aquí), se recorta a 0 para no invertir el signo.
  unsigned long duracionSonando = (duracionTotal > NON_LEGATO_GAP_MS) ? (duracionTotal - NON_LEGATO_GAP_MS) : 0;

  // Corta el LED antes de que termine el paso (separación fija non-legato),
  // dejando un pequeño hueco de silencio entre esta nota y la siguiente. El
  // paso como tal sigue durando lo mismo, así que el tempo/timing global no
  // se altera.
  if (!esSilencio && !notaSeparada && transcurrido >= duracionSonando) {
    enviarComando(secuenciaActual[currentStep].clientNumber, "OFF");
    clienteEncendidoActual = -1;
    notaSeparada = true;
  }

  // Ya se cumplió la duración total del paso: avanza al siguiente
  if (transcurrido >= duracionTotal) {
    if (!esSilencio && !notaSeparada) {
      // Respaldo por si la duración del paso es menor o igual al hueco de non-legato
      enviarComando(secuenciaActual[currentStep].clientNumber, "OFF");
      clienteEncendidoActual = -1;
    }
    stepEnCurso = false;

    if (currentStep + 1 >= largoActual) {
      // El villancico en turno terminó todos sus pasos por sí solo (sin
      // Stop). Se detiene y queda esperando Play/Siguiente/Anterior.
      secuenciaTerminada = true;
      isPlaying = false;
      Serial.println(">>> Villancico terminado. Presiona Play o Siguiente/Anterior.");
    } else {
      currentStep++;
      // El siguiente paso se inicia en la próxima llamada a actualizarSecuencia()
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

  // --- Botones (antirrebote 50ms) ---
  btnReset.setDebounceTime(50);
  btnPlay.setDebounceTime(50);
  btnPrev.setDebounceTime(50);
  btnNext.setDebounceTime(50);

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

  // Nivel 1 - reducir latencia: desactiva el power-save del WiFi,
  // que puede retrasar la entrega de paquetes hasta 100-300ms de forma
  // irregular si se deja en su modo por default.
  WiFi.setSleep(false);

  Serial.println();
  Serial.println("=================================");
  Serial.println("Conectado al router exitosamente");
  Serial.print("IP del ESP32 (maestro): ");
  Serial.println(WiFi.localIP());
  Serial.println("=================================");

  servidor.begin();
  Serial.println("Servidor iniciado. Esperando ESP32-C3...");
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

  // -------------------------------------------------------------------
  // STOP (GPIO 27): pausa la pista y apaga los LEDs ya, sin esperar
  // -------------------------------------------------------------------
  if (btnReset.isPressed()) {
    Serial.println(F("Accion: Stop"));
    myDFPlayer.pause();
    isPlaying = false;
    detenerLEDsInmediatamente();
  }

  // -------------------------------------------------------------------
  // PLAY (GPIO 12): (re)inicia la pista y la secuencia del villancico
  // en turno, desde el principio
  // -------------------------------------------------------------------
  if (btnPlay.isPressed()) {
    Serial.print(F("Accion: Play - "));
    Serial.println(villancicoNombres[currentVillancico]);
    myDFPlayer.playMp3Folder(currentVillancico + 1);
    isPlaying = true;
    iniciarSecuenciaDesdeElInicio();
  }

  // -------------------------------------------------------------------
  // ANTERIOR (GPIO 14): retrocede al villancico anterior (circular)
  // -------------------------------------------------------------------
  if (btnPrev.isPressed()) {
    if (isPlaying) detenerLEDsInmediatamente();

    currentVillancico = (currentVillancico == 0) ? (NUM_VILLANCICOS - 1) : (currentVillancico - 1);
    Serial.print(F("Villancico seleccionado: "));
    Serial.println(villancicoNombres[currentVillancico]);

    if (isPlaying) {
      Serial.println(F("-> Reproduciendo villancico anterior..."));
      myDFPlayer.playMp3Folder(currentVillancico + 1);
      iniciarSecuenciaDesdeElInicio();
    } else {
      Serial.println(F("-> Cambiado en silencio. Presiona PLAY."));
    }
  }

  // -------------------------------------------------------------------
  // SIGUIENTE (GPIO 13): avanza al villancico siguiente (circular)
  // -------------------------------------------------------------------
  if (btnNext.isPressed()) {
    if (isPlaying) detenerLEDsInmediatamente();

    currentVillancico = (currentVillancico + 1) % NUM_VILLANCICOS;
    Serial.print(F("Villancico seleccionado: "));
    Serial.println(villancicoNombres[currentVillancico]);

    if (isPlaying) {
      Serial.println(F("-> Reproduciendo villancico siguiente..."));
      myDFPlayer.playMp3Folder(currentVillancico + 1);
      iniciarSecuenciaDesdeElInicio();
    } else {
      Serial.println(F("-> Cambiado en silencio. Presiona PLAY."));
    }
  }

  // 4. Avanzar la secuencia de LEDs del villancico en turno (si está sonando)
  actualizarSecuencia();

  // 5. Apagar el LED de latencia del maestro cuando corresponda
  actualizarLedMaestro();
}
