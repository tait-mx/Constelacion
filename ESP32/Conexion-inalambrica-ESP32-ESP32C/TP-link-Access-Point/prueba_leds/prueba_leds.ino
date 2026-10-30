/*
  ESP32 (DevKit clásico) - MAESTRO de PRUEBA "Constelación"
  Enciende uno por uno el LED de cada cliente ESP32-C3.
  -------------------------------------------------------------------
    - PREV (GPIO 14) / NEXT (GPIO 13): seleccionan el LED (0 a 7, circular).
    - PLAY (GPIO 12): mientras se mantiene presionado, el LED seleccionado
      está encendido; al soltarlo se apaga.
    - LCD 16x2 I2C (0x27, SDA=21, SCL=22): muestra el número del LED
      seleccionado.

  Librerías necesarias: ezButton, LiquidCrystal_I2C.
*/

#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ezButton.h>

const char* ssid     = "constelacion";
const char* password = "01Constelacion01";
const uint16_t puerto = 8080;

// IP fija reservada para este maestro en el router (192.168.0.150)
IPAddress ipFija(192, 168, 0, 150);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

const int MAX_CLIENTES = 8;

WiFiServer servidor(puerto);
WiFiClient clientes[MAX_CLIENTES];

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------------------------------------------------------------
// BOTONES (a GND, INPUT_PULLUP interno vía ezButton)
// ---------------------------------------------------------------
#define BTN_PLAY_PIN 12
#define BTN_PREV_PIN 14
#define BTN_NEXT_PIN 13

ezButton btnPlay(BTN_PLAY_PIN);
ezButton btnPrev(BTN_PREV_PIN);
ezButton btnNext(BTN_NEXT_PIN);

// LED seleccionado (0 a MAX_CLIENTES-1)
uint8_t ledSeleccionado = 0;

// LED que está realmente encendido por PLAY (-1 = ninguno)
int8_t ledEncendido = -1;

// ---------------------------------------------------------------
// IDENTIFICACIÓN DE CLIENTES POR ÍNDICE FIJO ("ID:n")
// ---------------------------------------------------------------
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
          if (clientes[idx] && clientes[idx].connected()) {
            clientes[idx].stop();
          }
          clientes[idx] = pendientes[i];
          clientes[idx].setTimeout(100);
          pendientes[i] = WiFiClient();
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

    if (millis() - pendienteInicio[i] > TIMEOUT_IDENTIFICACION_MS) {
      Serial.print(">>> Timeout de identificación en slot temporal ");
      Serial.println(i);
      pendientes[i].stop();
    }
  }
}

// ---------------------------------------------------------------
// COMANDOS Y LCD
// ---------------------------------------------------------------
void enviarComando(uint8_t clientIndex, const char* comando) {
  if (clientIndex >= MAX_CLIENTES) return;
  if (clientes[clientIndex] && clientes[clientIndex].connected()) {
    clientes[clientIndex].println(comando);
    Serial.print("[Enviado: ");
    Serial.print(comando);
    Serial.print("] a cliente ");
    Serial.println(clientIndex);
  } else {
    Serial.print(">>> Cliente ");
    Serial.print(clientIndex);
    Serial.println(" no conectado");
  }
}

// Escribe una fila rellenando con espacios (evita lcd.clear() y su parpadeo).
void lcdImprimirFila(uint8_t fila, const char* texto) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", texto);
  lcd.setCursor(0, fila);
  lcd.print(buf);
}

void actualizarLCD() {
  char buf[17];
  snprintf(buf, sizeof(buf), "%u", ledSeleccionado);
  lcdImprimirFila(0, "LED seleccionado");
  lcdImprimirFila(1, buf);
}

// ---------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin();
  lcd.init();
  lcd.backlight();

  btnPlay.setDebounceTime(50);
  btnPrev.setDebounceTime(50);
  btnNext.setDebounceTime(50);

  WiFi.mode(WIFI_STA);
  WiFi.config(ipFija, gateway, subnet);
  WiFi.begin(ssid, password);

  Serial.print("Conectando a ");
  Serial.println(ssid);
  lcdImprimirFila(0, "Conectando WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  WiFi.setSleep(false);

  Serial.println();
  Serial.print("Conectado. IP del maestro: ");
  Serial.println(WiFi.localIP());

  servidor.begin();
  Serial.println("Servidor iniciado. Esperando ESP32-C3...");

  actualizarLCD();
}

void loop() {
  aceptarConexionNueva();
  procesarIdentificaciones();

  btnPlay.loop();
  btnPrev.loop();
  btnNext.loop();

  // ANTERIOR: LED seleccionado - 1 (circular)
  if (btnPrev.isPressed()) {
    ledSeleccionado = (ledSeleccionado == 0) ? (MAX_CLIENTES - 1) : (ledSeleccionado - 1);
    Serial.print("LED seleccionado: ");
    Serial.println(ledSeleccionado);
    actualizarLCD();
  }

  // SIGUIENTE: LED seleccionado + 1 (circular)
  if (btnNext.isPressed()) {
    ledSeleccionado = (ledSeleccionado + 1) % MAX_CLIENTES;
    Serial.print("LED seleccionado: ");
    Serial.println(ledSeleccionado);
    actualizarLCD();
  }

  // PLAY presionado: enciende el LED seleccionado
  if (btnPlay.isPressed()) {
    ledEncendido = ledSeleccionado;
    enviarComando(ledEncendido, "ON");
  }

  // PLAY soltado: apaga el LED que se encendió (aunque la selección haya cambiado)
  if (btnPlay.isReleased()) {
    if (ledEncendido != -1) {
      enviarComando(ledEncendido, "OFF");
      ledEncendido = -1;
    }
  }
}
