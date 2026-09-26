/*
  ESP32 (DevKit clásico) - CLIENTE del router + CONTROLADOR DE LED REMOTO (multi-cliente)
  -------------------------------------------------------------------
  Se conecta al router TP-Link (en vez de crear su propia red) y manda
  comandos "ON" y "OFF" a los ESP32-C3 conectados, siguiendo una
  secuencia programable (array de pasos: cliente + duración en fracción de beat).

  LED de latencia (maestro):
  Cada vez que se envía el comando "ON" a un cliente, el LED local del
  maestro (pin 2) se enciende al mismo tiempo, para poder ver a simple
  vista el retraso entre el encendido del maestro y el del cliente.
  El LED del maestro se apaga automáticamente tras la MITAD de la
  duración que se le mandó al cliente para ese paso (no espera a que
  el cliente reciba su "OFF").

  Cómo probarlo:
  1. Sube este sketch al ESP32 clásico.
  2. Sube el sketch cliente a cada ESP32-C3, apuntando al SSID del router
     y a la IP fija 192.168.0.150.
  3. Los LEDs de los C3 deberían encenderse uno por uno siguiendo la secuencia,
     y el LED del maestro debería parpadear brevemente al inicio de cada uno.
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

#define BPM 120
// Notación musical estándar: BPM define la duración de la NEGRA (1/4).
// Una nota entera (1/1) equivale a 4 negras.
#define MS_PER_WHOLE_NOTE (240000.0 / BPM)  // a 60bpm, 1/4 (negra) = 1000ms

// Formato de entrada: {clientNumber, numerador, denominador}
// clientNumber: 0 a 3 (índice en el array `clientes`)
// numerador/denominador: fracción de beat, ej. {1,4} = 1/4 de beat
struct StepFraction {
  uint8_t clientNumber;
  uint8_t numerator;
  uint8_t denominator;
};

StepFraction sequence1Raw[] = {
  {0, 1, 4},
  {1, 1, 4},
  {2, 1, 4},
  {3, 1, 4},
  {4, 1, 4},
  {5, 1, 4},
  {6, 1, 4},
  {7, 1, 4}
};
const uint8_t sequenceLength = sizeof(sequence1Raw) / sizeof(sequence1Raw[0]);

// Secuencia ya convertida a milisegundos (se llena en setup())
struct Step {
  uint8_t clientNumber;
  unsigned long durationMs;
};

Step sequence1[sequenceLength];

void convertirSecuencia() {
  for (int i = 0; i < sequenceLength; i++) {
    sequence1[i].clientNumber = sequence1Raw[i].clientNumber;
    float fraccion = (float)sequence1Raw[i].numerator / (float)sequence1Raw[i].denominator;
    sequence1[i].durationMs = (unsigned long)(fraccion * MS_PER_WHOLE_NOTE);
  }
}

uint8_t currentStep = 0;
unsigned long stepStartTime = 0;
bool stepLedIsOn = false;

void enviarComando(uint8_t clientIndex, const char* comando) {
  if (clientIndex >= MAX_CLIENTES) return;
  if (clientes[clientIndex] && clientes[clientIndex].connected()) {
    clientes[clientIndex].println(comando);
    Serial.print("[Enviado: ");
    Serial.print(comando);
    Serial.print("] a cliente ");
    Serial.println(clientIndex);
  }
}

void actualizarSecuencia() {
  unsigned long ahora = millis();

  if (!stepLedIsOn) {
    // Inicia el paso actual: enciende el LED del cliente correspondiente
    enviarComando(sequence1[currentStep].clientNumber, "ON");
    stepLedIsOn = true;
    stepStartTime = ahora;

    // Enciende también el LED local del maestro (medio tiempo del cliente)
    encenderLedMaestro(sequence1[currentStep].durationMs);
    return;
  }

  // Ya está encendido: revisa si terminó su duración
  if (ahora - stepStartTime >= sequence1[currentStep].durationMs) {
    enviarComando(sequence1[currentStep].clientNumber, "OFF");
    stepLedIsOn = false;
    currentStep = (currentStep + 1) % sequenceLength;
    // El siguiente paso se inicia en la próxima llamada a actualizarSecuencia()
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

  // 3. Avanzar la secuencia programada
  actualizarSecuencia();

  // 4. Apagar el LED de latencia del maestro cuando corresponda
  actualizarLedMaestro();
}
