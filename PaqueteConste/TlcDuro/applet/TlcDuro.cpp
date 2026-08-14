#include "Tlc5940.h"

#include "WProgram.h"
void setup();
void loop();
int inByte;
int led = 0;
int a[]={0,15,16,31};

void setup() {
  Tlc.init(0);
}

void loop() {
  
  for(int i=0; i<=3; i++)
  {
      Tlc.set(a[i],254);
      Tlc.update();
      delay(100);
      Tlc.set(a[i],0);
      Tlc.update();
      delay(100);
  }
  
}


int main(void)
{
	init();

	setup();
    
	for (;;)
		loop();
        
	return 0;
}

