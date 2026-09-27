/*
  ESP32 (DevKit clásico) - CLIENTE del router + CONTROLADOR DE LED REMOTO (multi-cliente)
  -------------------------------------------------------------------
  Se conecta al router TP-Link (en vez de crear su propia red) y manda
  comandos "ON" y "OFF" a los ESP32-C3 conectados, siguiendo una
  secuencia programable (array de pasos: cliente + duración en fracción de beat).

  Villancicos: "Noche de Paz", "El Niño del Tambor" y "Joy to the World".
  Los tres se concatenan en una sola secuencia y se reproducen uno detrás de
  otro, EN ESE ORDEN, UNA SOLA VEZ. Al terminar el último paso del último
  villancico la secuencia se detiene por completo (sin loop).

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

  Cómo probarlo:
  1. Sube este sketch al ESP32 clásico.
  2. Sube el sketch cliente a cada ESP32-C3, apuntando al SSID del router
     y a la IP fija 192.168.0.150.
  3. Los LEDs de los C3 deberían encenderse uno por uno siguiendo la
     secuencia de los tres villancicos en orden, respetando silencios y
     la separación non-legato, y el LED del maestro debería parpadear
     brevemente al inicio de cada nota (pero no durante los silencios).
     Al terminar "Joy to the World" la secuencia se detiene.
*/

#include <WiFi.h>

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

#define BPM 120
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
// SELECCIÓN Y ORDEN DE LOS VILLANCICOS
// ---------------------------------------------------------------
// villancicos[]: arreglo de PUNTEROS a cada melodía definida arriba. El
// orden de este arreglo es el orden de reproducción: primero se toca por
// completo villancicos[0], luego villancicos[1], luego villancicos[2].
// Como los tres arreglos tienen distinto tamaño, cada entrada de
// villancicoLengths[] guarda cuántos pasos tiene el villancico
// correspondiente en el mismo índice (necesario porque sizeof() de un
// puntero no sirve para saber el largo del arreglo al que apunta).
StepFraction* villancicos[] = { nocheDePaz, ninoDelTambor, joyToTheWorld };
const uint16_t villancicoLengths[] = {
  sizeof(nocheDePaz) / sizeof(nocheDePaz[0]),
  sizeof(ninoDelTambor) / sizeof(ninoDelTambor[0]),
  sizeof(joyToTheWorld) / sizeof(joyToTheWorld[0])
};
const char* villancicoNombres[] = { "Noche de Paz", "El Nino del Tambor", "Joy to the World" };
const uint8_t NUM_VILLANCICOS = sizeof(villancicos) / sizeof(villancicos[0]);

// Capacidad máxima del arreglo final: la suma de los tres villancicos
// (261 pasos en total). Es una cota fija en tiempo de compilación, por eso
// se calcula directo con sizeof() en vez de sumar villancicoLengths[] en
// un for (que no sería una expresión constante).
const uint16_t CAPACIDAD_SECUENCIA =
  (sizeof(nocheDePaz) / sizeof(nocheDePaz[0])) +
  (sizeof(ninoDelTambor) / sizeof(ninoDelTambor[0])) +
  (sizeof(joyToTheWorld) / sizeof(joyToTheWorld[0]));

// Secuencia ya concatenada y convertida a milisegundos (se llena en setup())
struct Step {
  uint8_t clientNumber;
  unsigned long durationMs;
};

Step sequence1[CAPACIDAD_SECUENCIA];

// Cantidad real de pasos usados en sequence1 (según villancicoLengths[]).
// Es la misma que CAPACIDAD_SECUENCIA mientras se toquen los 3 villancicos
// completos; queda separada por si en el futuro se decide tocar solo
// algunos o repetir alguno, sin tener que cambiar el tamaño del arreglo.
uint16_t sequenceLengthReal = 0;

// Índice (dentro de sequence1) en el que empieza cada villancico, para
// poder avisar por Serial cuándo empieza cada uno.
uint16_t inicioDeVillancico[NUM_VILLANCICOS];

void convertirSecuencia() {
  uint16_t destIndex = 0;
  for (uint8_t v = 0; v < NUM_VILLANCICOS; v++) {
    inicioDeVillancico[v] = destIndex;
    StepFraction* villancico = villancicos[v];
    uint16_t largoVillancico = villancicoLengths[v];
    for (uint16_t i = 0; i < largoVillancico; i++) {
      sequence1[destIndex].clientNumber = villancico[i].clientNumber;
      float fraccion = (float)villancico[i].numerator / (float)villancico[i].denominator;
      sequence1[destIndex].durationMs = (unsigned long)(fraccion * MS_PER_WHOLE_NOTE);
      destIndex++;
    }
  }
  sequenceLengthReal = destIndex;
}

uint16_t currentStep = 0;
unsigned long stepStartTime = 0;
bool stepEnCurso = false;
bool notaSeparada = false;    // true una vez que ya se mandó el OFF anticipado (non-legato)
bool secuenciaTerminada = false;  // true cuando ya se tocaron los 3 villancicos (sin loop)

void enviarComando(uint8_t clientIndex, const char* comando) {
  if (clientIndex >= MAX_CLIENTES) return;  // cubre también SILENCE (255)
  if (clientes[clientIndex] && clientes[clientIndex].connected()) {
    clientes[clientIndex].println(comando);
    Serial.print("[Enviado: ");
    Serial.print(comando);
    Serial.print("] a cliente ");
    Serial.println(clientIndex);
  }
}

void actualizarSecuencia() {
  // Una vez que se tocaron los 3 villancicos ya no hay nada más que hacer:
  // NO se reinicia currentStep ni se vuelve a recorrer la secuencia (sin loop).
  if (secuenciaTerminada) return;

  unsigned long ahora = millis();
  bool esSilencio = (sequence1[currentStep].clientNumber == SILENCE);
  unsigned long duracionTotal = sequence1[currentStep].durationMs;

  if (!stepEnCurso) {
    // Aviso por Serial cuando currentStep cae justo en el inicio de un villancico
    for (uint8_t v = 0; v < NUM_VILLANCICOS; v++) {
      if (currentStep == inicioDeVillancico[v]) {
        Serial.print(">>> Iniciando villancico: ");
        Serial.println(villancicoNombres[v]);
        break;
      }
    }

    // Inicia el paso actual.
    if (esSilencio) {
      Serial.println("[Silencio]");
    } else {
      // Enciende el LED del cliente correspondiente a la nota
      enviarComando(sequence1[currentStep].clientNumber, "ON");
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
    enviarComando(sequence1[currentStep].clientNumber, "OFF");
    notaSeparada = true;
  }

  // Ya se cumplió la duración total del paso: avanza al siguiente
  if (transcurrido >= duracionTotal) {
    if (!esSilencio && !notaSeparada) {
      // Respaldo por si la duración del paso es menor o igual al hueco de non-legato
      enviarComando(sequence1[currentStep].clientNumber, "OFF");
    }
    stepEnCurso = false;

    if (currentStep + 1 >= sequenceLengthReal) {
      // Se tocaron los 3 villancicos completos: detener la secuencia (sin loop)
      secuenciaTerminada = true;
      Serial.println(">>> Secuencia completa: los 3 villancicos terminaron. No hay loop.");
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

  // 3. Avanzar la secuencia programada (los 3 villancicos, sin loop al terminar)
  actualizarSecuencia();

  // 4. Apagar el LED de latencia del maestro cuando corresponda
  actualizarLedMaestro();
}
