#include "Tlc5940.h"

int inByte;
int led = 0;
int a[]={
  0,15,16,31};

void setup() {
  Tlc.init(0);
}
/*
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
 */
/*
void loop() {
 for(int f=0; f<=1; f++) {   
 for(int i=0; i<=3; i++) {
 Tlc.set(a[i],f*254);
 Tlc.update();
 }
 delay(500);
 }
 }
 */


/*
void loop() {
  
  Tlc.set(0,254);
  Tlc.update();
  Tlc.set(15,254);
  Tlc.update();
  Tlc.set(16,254);
  Tlc.update();
  Tlc.set(31,254);
  Tlc.update();
  delay(500);

  Tlc.set(0,0);
  Tlc.update();
  Tlc.set(15,0);
  Tlc.update();
  Tlc.set(16,0);
  Tlc.update();
  Tlc.set(31,0);
  Tlc.update();
  delay(500);
}
*/



