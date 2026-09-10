#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>

// Usamos el puerto Serie 2 del ESP32
// RX2 = GPIO 16, TX2 = GPIO 17
HardwareSerial mySerial(2);
DFRobotDFPlayerMini myDFPlayer;

void setup() {
  Serial.begin(115200);
  
  // Inicializamos HardwareSerial (RX=16, TX=17)
  mySerial.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println(F("Inicializando DFPlayer Mini..."));

  if (!myDFPlayer.begin(mySerial)) {
    Serial.println(F("Error al conectar con el DFPlayer. Revisa conexiones."));
    while (true); // Detener ejecución si falla
  }

  Serial.println(F("DFPlayer listo."));
  
  // Ajusta el volumen (0 a 30)
  myDFPlay
  er.volume(20);
  
  delay(500);

  // Reproduce la primera pista de la carpeta '01' (01/001.mp3)
  Serial.println(F("Enviando comando PLAY..."));
  myDFPlayer.playMp3Folder(1);
}

void loop() {
  // Sin acciones continuas por ahora
}
