#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <stdlib.h>
#include "global.h"
#include "rf12.h"
#include "uart.h"

#include <stdio.h>

#include <util/delay.h>

//Rx


int main(void)
{

	sei();

	UART_Init();
	rf12_init();				// ein paar Register setzen (z.B. CLK auf 10MHz)
	UART_Tx_Str("Init\n", 5);
	rf12_setfreq(RF12FREQ(433.92));		// Sende/Empfangsfrequenz auf 433,92MHz einstellen
	rf12_setbandwidth(4, 1, 4);		// 200kHz Bandbreite, -6dB Verstärkung, DRSSI threshold: -79dBm 
	rf12_setbaud(19200);			// 19200 baud
	rf12_setpower(0, 6);			// 1mW Ausgangangsleistung, 120kHz Frequenzshift

	for (;;)
	{
		unsigned char ret;
		unsigned char test[50];
/* none blocking sample */	
		unsigned char test2[50];

		UART_Tx_Str(test, sprintf(test, "Start val: %u\n", rf12_rxstart()));

		ret = 255;				//not completed yet
		while(ret == 255)
		{
			ret = rf12_rxfinish(test2);	//try if transfer completed
		}
		UART_Tx_Str(test, sprintf(test, "ret: %u\n", ret));
		if(ret != 0 && ret != 254)				//no CRC error && no old str
			UART_Tx_Str(test2, ret);

/*blocking sample */	
/*		ret = rf12_rxdata(test);
		if(ret)
			UART_Tx_Str(test,ret);
		else
			UART_Tx_Str("CRC Err\n", 8);
*/	}
}
