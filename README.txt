

README  



//////////////////////////////////////////////////////////////////////
26 de agosto de 2026

CÓDIGO ACTUAL
Constelacion-Intro-LCD.ino

While the intro is running (descendent scale) display "La Constelación". Then display current song

Elementos sin cambios

Cambios en Botones:

if  the mp3 device is playing 

button3 pressed goes to previous track and plays it
button4 pressed goes to next track and plays it
if  the mp3 is not playing

button3 pressed goes to previous track but doesn´t play
button4 pressed goes to next track but doesn´t play
In any case the villancico name must be displayed in the LCD



//////////////////////////////////////////////////////////////////////
25 de agosto de 2026

CÓDIGO ACTUAL
Constelacion-Tlc-DFPlayer-Villancicos-LCD-names.ino

Fix setup. Display villancico name in LCD
Moved lcd.begin(16, 2); ot ouf the for() {} in thev setup() section to
avoid unnecessary iterarions
The villancico currently playing or ready to be played displays in the
LCD

Al iniciar el código los villancicos no empiezan automáticamente sino haste que le das play

Elementos:
Arduino Mega 1280 : Programa, secuenciador de los LEDs,  ejecución de la pista mp3, sincronización.
Tlc5940NT: Multiplexor para controlar los LEDs
DFPlayer mini: MP3 player que reproduce una pista musical de acompañamiento 
LCD1602: Muestra un texto de prueba Línea 1 -> "La Constelacion", Línea 2 -> "Villancicos"
4 push buttons: Control general de la ejecución

Función de los botones
Los botones actúan de la misma manera con el MP3 y con la secuencia de LEDs de forma sincronizada
botón 1 STOP – detiene la canción
botón 2 PLAY – Inicia la reproduccón de la canción actual
botón 3 PREVIOUS – Retrocede a la  canción anterior y la reproduce convirtiéndose en la canción actual
botón 4 NEXT – Avanza a la siguiente canción y la reproduce convirtiéndose en la canción actual

Para usar el Tlc5940 con el Arduino Mega hay que usar una fuente de 12vcc. Si no los LEDs no encienden bien.




//////////////////////////////////////////////////////////////////////
21 de agosto de 2026

CÓDIGO ACTUAL
Constelacion-Tlc-DFPlayer-Villancicos-LCDtest.ino

Elementos:
Arduino Mega 1280 : Programa, secuenciador de los LEDs,  ejecución de la pista mp3, sincrinización.
Tlc5940NT: Multiplexor para controlar los LEDs
DFPlayer mini: MP3 player que reproduce una pista musical de acompañamiento 
LCD1602: Muestra un texto de prueba Línea 1 -> "La Constelacion", Línea 2 -> "Villancicos"
4 push buttons: Control general de la ejecución

Función de los botones
Los botones actúan de la misma manera con el MP3 y con la secuencia de LEDs de forma sincronizada
botón 1 STOP – detiene la canción
botón 2 PLAY – Inicia la reproduccón de la canción actual
botón 3 PREVIOUS – Retrocede a la  canción anterior y la reproduce convirtiéndose en la canción actual
botón 4 NEXT – Avanza a la siguiente canción y la reproduce convirtiéndose en la canción actual

Para usar el Tlc5940 con el Arduino Mega hay que usar una fuente de 12vcc. Si no los LEDs no encienden bien.



//////////////////////////////////////////////////////////////////////
20 de agosto de 2026

CÓDIGO ACTUAL
Constelacion-Tlc-DFPlayer-Villancicos-Debounce.ino

Elementos:
Arduino Mega 1280 : Programa, secuenciador de los LEDs,  ejecución de la pista mp3, sincrinización.
Tlc5940NT: Multiplexor para controlar los LEDs
DFPlayer mini: MP3 player que reproduce una pista musical de acompañamiento 
4 push buttons: Control general de la ejecución

Función de los botones
Los botones actúan de la misma manera con el MP3 y con la secuencia de LEDs de forma sincronizada
botón 1 STOP – detiene la canción
botón 2 PLAY – Inicia la reproduccón de la canción actual
botón 3 PREVIOUS – Retrocede a la  canción anterior y la reproduce convirtiéndose en la canción actual
botón 4 NEXT – Avanza a la siguiente canción y la reproduce convirtiéndose en la canción actual

Para usar el Tlc5940 con el Arduino Mega hay que usar una fuente de 12vcc. Si no los LEDs no encienden bien.