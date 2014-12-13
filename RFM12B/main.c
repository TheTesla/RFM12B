#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <stdlib.h>
#include "global.h"
#include "rf12.h"     

#define F_CPU 10000000UL
#include <util/delay.h>

void send(void);
void receive(void);

int main(void)
{
	rf12_init();					// ein paar Register setzen (z.B. CLK auf 10MHz)
	rf12_setfreq(RF12FREQ(433.92));	// Sende/Empfangsfrequenz auf 433,92MHz einstellen
	rf12_setbandwidth(4, 1, 4);		// 200kHz Bandbreite, -6dB Verstärkung, DRSSI threshold: -79dBm 
	rf12_setbaud(19200);			// 19200 baud
	rf12_setpower(0, 6);			// 1mW Ausgangangsleistung, 120kHz Frequenzshift

	for (;;)
	{
		send();
		for (unsigned char i=0; i<100; i++)
			_delay_ms(10);

	//	receive();
	}
}

void receive(void)
{	unsigned char test[32];	
	rf12_rxdata(test,32);	
	// daten verarbeiten
}

void send(void)
{	unsigned char test[]="Dies ist ein 433MHz Test !!!\n   ";	
	rf12_txdata(test,32);
}

