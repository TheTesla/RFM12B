#define MCU AtMega32 


#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <avr/sleep.h>
#include "RFM12Bdrv.h"
#include <string.h>


#define F_CPU 8000000UL  // Systemtakt in Hz - Definition als unsigned long beachten 
                         // Ohne ergeben sich unten Fehler in der Berechnung

 
#define BAUD 38400UL      // Baudrate
 
#define RF12_BAUD 1200 
#define RF12_FREQ 433.92

#define EMPF


// Berechnungen
#define UBRR_VAL ((F_CPU+BAUD*8)/(BAUD*16)-1)   // clever runden
#define BAUD_REAL (F_CPU/(16*(UBRR_VAL+1)))     // Reale Baudrate
#define BAUD_ERROR ((BAUD_REAL*1000)/BAUD) // Fehler in Promille, 1000 = kein Fehler.

const uint8_t CQ_ChCh[] ={0xAA, 0xAA, 0xAA, 0x2D, 0xD4, ' ', 'M', 'O', ' ', 'C', 'Q', ' ', 'C', 'h', 'C', 'h', 0, 0};
const uint8_t ACK_ChCh[] ={0xAA, 0xAA, 0xAA, 0x2D, 0xD4, ' ', 'M', 'O', ' ', 'A', 'C', 'K', ' ', 'C', 'h', 'C', 'h', 0, 0};

char gRXframe[256];
uint8_t gRXptr;


uint8_t gOperation;

void uart_init(void)
{
  UBRRH = UBRR_VAL >> 8;
  UBRRL = UBRR_VAL & 0xFF;
 
  UCSRB |= (1<<TXEN);  // UART TX einschalten
  UCSRC = (1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0);  // Asynchron 8N1 
}

void LED1(uint8_t led)
{
	DDRD |= (1 << 6);
	if(led) PORTD|= (1 << 6);
	else	PORTD&= ~(1 << 6);
}	

void LED2(uint8_t led)
{
	DDRD |= (1 << 5);
	if(led) PORTD|= (1 << 5);
	else	PORTD&= ~(1 << 5);
}	


void SendACK(void)
{
	rf12_frame_handle_t ACK_frame[2];
	ACK_frame[0].TXptr = (uint8_t*) ACK_ChCh;
	ACK_frame[0].TXoctets = 19;
	ACK_frame[0].RF_config = RF12_easyconfig(RF12_FREQ, RF12_BAUD, 0, 1, 0);//RF12myconfig;
	ACK_frame[1].TXptr = 0;
	ACK_frame[1].TXoctets = 0;
	ACK_frame[1].RF_config = RF12_easyconfig(RF12_FREQ, RF12_BAUD, 0, 0, 1); //RF12myconfigRX;
	rf12_sendframe(ACK_frame);
}	

void RF12_off(void)
{
	rf12_frame_handle_t OFF_frame[1];
	OFF_frame[0].TXptr = 0;
	OFF_frame[0].TXoctets = 0;
	OFF_frame[0].RF_config = RF12_easyconfig(RF12_FREQ, RF12_BAUD, 0, 0, 0); //RF12myconfigRX;
	// Funkmodul ausschalten
	OFF_frame[0].RF_config.Recen = 0;
	OFF_frame[0].RF_config.basebanden = 0;
	OFF_frame[0].RF_config.TXen = 0;
	OFF_frame[0].RF_config.Synthen = 0;
	OFF_frame[0].RF_config.Xtalen = 0;
	OFF_frame[0].RF_config.brownouten = 0;
	OFF_frame[0].RF_config.wakeupen = 0;
	OFF_frame[0].RF_config.CLKoutdis = 1;
	// zyklischer wakeup
	//OFF_frame[0].RF_config.wakeupen = 1;
	
	rf12_sendframe(OFF_frame);
}	

void RXfunc(uint8_t recv_octet)
{
	UDR = recv_octet;
	gRXptr++;
	gRXframe[gRXptr] = recv_octet;
	if(gRXptr > 17){
		gRXptr = 0;
		RF12_INT_OFF
		rf12_cmdarray(2, RXcmdarray);
		gRXframe[0] = 'X';
		gRXframe[18] = 0;
		if(strstr(gRXframe,"CQ ChCh")){
			gOperation = 2;
			SendACK();
		}
		if(strstr(gRXframe,"ACK ChCh")){
			gOperation = 0;
			#ifndef EMPF	
				RF12_off(); // Energie sparen - Empfänger aus, nur Klingelknopf - Auskommentieren für Klingel-Empfänger
			#endif		
		}			
	}	
}	

ISR(TIMER0_OVF_vect) /* veraltet: SIGNAL(SIG_OVERFLOW0) */
{
    /* Interrupt Code */
}

int main(void)
{
	gRXptr = 0;
	uint16_t test;
	uint32_t w;
	uint32_t v;
	LED2(0);



	RF12_init();


	uart_init();
	UDR = 'A';
	test = rf12_cmd_transfer_blocking(0xFE00);
	//wichtig! nach softreset warten
	for(w=0;w<3000;w++){
		LED1(1);
	}			

	LED1(0);

	// Bei ds (double speed) meist Störung
	SPI_ns
	
	DDRD |= (1 << 7);
	PORTD &= ~(1 << 7);
	
	//Funktion, die gerufen wird, wenn ein Octet empfangen wurde
	grf12_recv_fp = RXfunc;
	
	//Funktion, die nach dem Aussenden eines Frames gerufen wird
	grf12_frame_rdy_fp = rf12_activateRX;
	
	for(w=0;w<3000;w++){
		LED1(1);
	}	
	
	

	

	rf12_frame_handle_t RF12_example_frame[2];
	RF12_example_frame[0].TXptr = (uint8_t*) CQ_ChCh;
	RF12_example_frame[0].TXoctets = 16;
	RF12_example_frame[0].RF_config = RF12_easyconfig(RF12_FREQ, RF12_BAUD, 0, 1, 0);//RF12myconfig;
	RF12_example_frame[1].TXptr = 0;
	RF12_example_frame[1].TXoctets = 0;
	RF12_example_frame[1].RF_config = RF12_easyconfig(RF12_FREQ, RF12_BAUD, 0, 0, 1); //RF12myconfigRX;



	LED1(0);
	
	PORTA = 0xFF;
	DDRA = 0;
	
	#ifdef EMPF
		gOperation = 0;
		rf12_sendframe(RF12_example_frame);
	#else
		gOperation = 1;
	#endif
	
	while(1){
		if(0 == (PINA & (1 << 0))){
			gOperation = 1;
		}			
		if(1 == gOperation){
			rf12_sendframe(RF12_example_frame);
			for(w=0;w<2000;w++) LED1(1);
			for(w=0;w<2000;w++) LED1(0);
		}
		if(2 == gOperation){
			// Klingeln
			DDRD |= (1 << 7);
			/*
			for(v=0;v<100;v++){ 
				for(w=0;w<40;w++) PORTD |= (1 << 7);
				for(w=0;w<40;w++) PORTD &= ~(1 << 7);
			}
			*/
			for(w=0;w<1500;w++) PORTD |= (1 << 7);
			PORTD &= ~(1 << 7);
			gOperation = 0;
		}		
	}



	return 0;
}


