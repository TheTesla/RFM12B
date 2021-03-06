#include <avr/io.h>
#include <avr/interrupt.h>
#include "global.h"
#include "rf12.h"
#include "uart.h"

#include <util/delay.h>

void send(void);

//Tx

int main(void)
{
	DDRD=0;
	sei();
	UART_Init();
	rf12_init();					// ein paar Register setzen (z.B. CLK auf 10MHz)
	UART_Tx_Str("Init\n", 5);
	rf12_setfreq(RF12FREQ(433.92));			// Sende/Empfangsfrequenz auf 433,92MHz einstellen
	rf12_setbandwidth(RxBW200, LNA_6, RSSI_79);	// 200kHz Bandbreite, -6dB Verstärkung, DRSSI threshold: -79dBm 
	rf12_setbaud(19200);				// 19200 baud
	rf12_setpower(PWRdB_0, TxBW120);		// 1mW Ausgangangsleistung, 120kHz Frequenzshift


	while(1)
	{
		unsigned char i;
		UART_Tx_Str("Pkg\n", 4);
		send();
		for (i=0; i<100; i++)
			_delay_ms(10);
	}
}

void send(void)
{	
	unsigned char test[]="Dies ist ein 433MHz Test !!!\n";	
/* Blocking sample */
//	rf12_txdata(test,29);
/* None blocking sample */
	UART_Tx_Str("Start...", 8);
	rf12_txstart(test, 29);
	while(rf12_txfinished());
	UART_Tx_Str("Done\n", 5);
}
