#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#define MCU AtMega32 

#if MCU==AtMega32
	#define DDR_SPI DDRB
	#define DD_MOSI 5
	#define DD_SCK  7
	#define DD_MISO 6
	#define DD_SS	4
	#define PORT_SPI PORTB
	#define PORT_SS 4
	#define FSK_PORT PORTB
	#define FSK_DDR DDRB
	#define FSK_PIN PINB
	#define FSK		3
#endif

#define rf12_trans SPI_transfer16_blocking

#define RF12FREQ(freq)	((freq-430.0)/0.0025)							// macro for calculating frequency value out of frequency in MHz


volatile uint16_t gspival;
volatile uint8_t gSPI_readval_L;
volatile uint8_t gSPI_readval_H;
volatile uint8_t gSPI_ready;
volatile uint8_t grf12_remaining_bytes_to_transmit;
volatile uint8_t *grf12_transmitptr;
volatile uint8_t grf12_PM;
volatile uint8_t grf12_TX_running;
volatile uint8_t gi;

void LED1(uint8_t led)
{
	DDRD |= (1 << 6);
	if(led) PORTD|= (1 << 6);
	else	PORTD&= ~(1 << 6);
}	


void SPI_MasterInit( void)
{
	/* Set MOSI and SCK output, all others input */
	DDR_SPI = (1<<DD_MOSI)|(1<<DD_SCK)|(1<<DD_SS); // SS-Output Softwaregesteuert
	/* Enable SPI, Master, set clock rate fck/16 */
	SPCR = (1<<SPIE)|(1<<SPE)|(1<<MSTR)|(1<<SPR1)|(1<<SPR0);
}
void SPI_MasterTransmit(char cData)
{
	/* Start transmission */
 	SPDR = cData;
}

//Interrupt nach Fertigstellung des ersten SPI-Byte -> zweites senden
ISR(SPI_STC_vect)
{
	//SPSR = 1 << SPIF; // Interrupt-Flag zurücksetzen
	if(gSPI_ready == 1){

		gSPI_ready = 2;			
		PORT_SPI |= (1 << PORT_SS); //Slave select Ende
		//PORT_SPI &= ~(1 << PORT_SS); //Slave select Ende toggle
		SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0); // SPI-Interrupt nach Transfer abschalten
		if(grf12_TX_running == 1) {
			GIMSK |= (1 << INT0); // Interrupt INT0 aktivieren, wenn Array-Transfer, damit nächstes Byte geholt wird.	
		}		
		//GIMSK |= (1 << INT0); // Interrupt INT0 aktivieren
	}
	else{		
		gSPI_readval_H = SPDR;
		SPI_MasterTransmit(gspival % 256); // letztes Byte
		gSPI_ready = 1;
	}	
}

//Funktion zum Senden eines 16-Bit Wertes über SPI
void SPI_transfer16(uint16_t spival)
{
	gSPI_ready = 0;
	PORT_SPI &= ~(1 << PORT_SS);//Slave select Start
	SPI_MasterInit();
	gspival = spival; 
	SPI_MasterTransmit(gspival >> 8); // erstes Byte

}


void rf12_PM_nonblock(uint8_t CW)
{
	SPI_transfer16(0x8200 | CW);
}	


void rf12_transmitbyte(uint8_t sendbyte)
{
	SPI_transfer16(0xB800 | sendbyte);	
}	 



//to do: nebenläufiges Senden von Vektoren
ISR(INT0_vect)
{
	GIMSK &= ~(1 << INT0); // Interrupt INT0 deaktivieren
	GIFR = (1 << INTF0);
	LED1(grf12_remaining_bytes_to_transmit % 1);
	//rf12_transmitbyte(*grf12_transmitptr++);
	//grf12_remaining_bytes_to_transmit--;
	if(0==grf12_remaining_bytes_to_transmit){
		//GIMSK &= ~(1 << INT0); // Interrupt INT0 deaktivieren	
		rf12_PM_nonblock(grf12_PM); // Transmitter aus
		//SPI_join();
		grf12_TX_running = 0; 
	}
	else{
		rf12_transmitbyte(*grf12_transmitptr++);
	}		
	grf12_remaining_bytes_to_transmit--;
}	

void SPI_join(void)
{
	while(2!=gSPI_ready){
		gi++;
		}; //warten bis zweites Byte an SPI-Schnittstelle übergeben
}	

uint16_t SPI_transfer16_blocking(uint16_t spival)
{
	SPI_transfer16(spival); // nicht blockierender Transfer als Basis
	SPI_join(); //warten bis beide Bytes übertragen
	gSPI_readval_L = SPDR;
	return (gSPI_readval_H << 8) | (gSPI_readval_L); // Rückgabewert des vorangegangen Kommandos
}	


void rf12_setfreq(unsigned short freq)
{	if (freq<96)				// 430,2400MHz
		freq=96;
	else if (freq>3903)			// 439,7575MHz
		freq=3903;
	rf12_trans(0xA000|freq);
}

void rf12_transmitarray(uint8_t n, uint8_t *data)
{
	grf12_TX_running = 1;
	grf12_transmitptr = data;
	grf12_remaining_bytes_to_transmit = n;
	rf12_PM_nonblock(grf12_PM | 0x20); // Transmitter an
	//GIMSK = (1 << INT0); // Interrupt INT0 aktiv
}	



int main(void)
{
	uint16_t test;
	sei();
	FSK_DDR |= (1 << FSK);
	FSK_PORT |= (1 << FSK);
	SPI_MasterInit();
	MCUCR = 0; // Low Level Interrupt (INT0, INT1)

	test = SPI_transfer16_blocking(0xFE00);
	test = SPI_transfer16_blocking(0x0000);
	test = SPI_transfer16_blocking(0x0000);
	test = SPI_transfer16_blocking(0x0000);
	//test = SPI_transfer16_blocking(0x9480);
	
	rf12_trans(0xC0E0);			// AVR CLK: 10MHz
	rf12_trans(0x8097);			// Disable FIFO
	rf12_trans(0xC2AB);			// Data Filter: internal
	rf12_trans(0xCA81);			// Set FIFO mode
	rf12_trans(0xE000);			// disable wakeuptimer
	rf12_trans(0xC800);			// disable low duty cycle
	rf12_trans(0xC400);//rf12_trans(0xC4F7);			// AFC settings: autotuning: -10kHz...+7,5kHz

	rf12_setfreq(RF12FREQ(433.92));
	//rf12_trans(0x8238);			// TX on
	grf12_PM = 0x08;
	//rf12_transmitbyte('H');
	//rf12_PM_nonblock(grf12_PM); // Transmitter aus
	LED1(1);
	SPI_join();
	LED1(0);
	//rf12_trans(0x8218);			// TX off
	LED1(1);
	//test = SPI_transfer16_blocking(0xB8AA);
	LED1(0);
	//while(1){test = SPI_transfer16_blocking(0xB8AA);}
	//test = SPI_transfer16_blocking(0xB000);
	//rf12_PM_nonblock(grf12_PM | 0x20); // Transmitter an
	//while(1);
	
	
	
	
	while(1){
		rf12_transmitarray(11, (uint8_t*)"Hallo Welt!");
		while(255-grf12_remaining_bytes_to_transmit){
			//LED1(1);
			}
		test = SPI_transfer16_blocking(0x0000);
		test = SPI_transfer16_blocking(0x0000);
		LED1(1);
		//SPI_join();
		LED1(0);
		rf12_PM_nonblock(0x8200); // Transmitter aus
		//test = SPI_transfer16_blocking(0xB8AA);
			test = SPI_transfer16_blocking(0xFE00);
		test = SPI_transfer16_blocking(0x0000);
		test = SPI_transfer16_blocking(0x0000);
		test = SPI_transfer16_blocking(0xB000);
		test = SPI_transfer16_blocking(0x0000);
		test = SPI_transfer16_blocking(0x0000);


		LED1(1);
		//SPI_join();
		LED1(0);
		
	}		 
		//test = SPI_transfer16_blocking(0x0000);

		//rf12_PM_nonblock(grf12_PM); // Transmitter aus
		SPI_join();
		LED1(1);
		SPI_join();
		LED1(0);
		//rf12_PM_nonblock(grf12_PM); // Transmitter aus
		LED1(1);
		SPI_join();
		LED1(0);	
			test = SPI_transfer16_blocking(0x0000);
				test = SPI_transfer16_blocking(0x0000);
					test = SPI_transfer16_blocking(0x0000);

			
	test = SPI_transfer16_blocking(0xB000);
	test = SPI_transfer16_blocking(0xB000);
	rf12_trans(0x8218);			// TX off
	
	while(1);

	return 0;
}