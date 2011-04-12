#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>

 void one() {
	 PORTC |= _BV(PINC5);
	 PORTC |= _BV(PINC2);
	 PORTC &= ~_BV(PINC2);
 }
 
 void zero() {
 	
	 PORTC &= ~_BV(PINC5);
	 PORTC |= _BV(PINC2);
	 PORTC &= ~_BV(PINC2);

}

void latch() {
	PORTC  |= _BV(PINC3);
	PORTC  &= ~_BV(PINC3);
}
int main (void)
{ 
   DDRC = 0b00111111;
	
	 PORTC |= _BV(PINC1);
	 PORTC &= ~_BV(PINC4);
	 one();
	 zero();
   one();
	 zero();
	 one();
	 one();
	 one();
	 zero();
	 latch();
   
   int i = 0;
   int d = 1;
   while (1) {
   	i += d;
   	if(i>7 && d) { d = -1; i += d;}
   	if(i<0) { d = 1; i += d;}
   	
   	if(i == 0) { one(); zero(); zero(); zero(); zero(); zero(); zero(); zero(); };
   	if(i == 1) { zero(); one(); zero(); zero(); zero(); zero(); zero(); zero(); };
   	if(i == 2) { zero(); zero(); one(); zero(); zero(); zero(); zero(); zero(); };
   	if(i == 3) { zero(); zero(); zero(); one();  zero(); zero(); zero(); zero(); };
   	if(i == 4) { zero(); zero(); zero(); zero();one();  zero(); zero(); zero(); };
   	if(i == 5) {  zero(); zero(); zero(); zero(); zero(); one(); zero(); zero(); };
   	if(i == 6) {  zero(); zero(); zero(); zero(); zero(); zero(); one(); zero(); };
   	if(i == 7) {  zero(); zero(); zero(); zero(); zero(); zero(); zero(); one(); };
   		
		/*
		i++;
		if((i&1)) { one(); } else { zero(); }
		if((i&2)) { one(); } else { zero(); }
		if((i&4)) { one(); } else { zero(); }
		if((i&8)) { one(); } else { zero(); }
	  if((i&16)) { one(); } else { zero(); }
	  if((i&32)) { one(); } else { zero(); }
	  if((i&64)) { one(); } else { zero(); }
	  if((i&128)) { one(); } else { zero(); }
	  */
   	/*
   	for(int i = 0; i < 8; i++) {
   		if(rand()%50) {
	  		zero();
	  	} else { 
	  		one();
	 	  }
  	}
  	*/
  	
  	latch();
    _delay_ms(40);
   }
}