

README  
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



1 Para usar el Tlc5940 con el Arduino Mega hay que usar una fuente de 12vcc. Si no los LEDs no encienden bien.