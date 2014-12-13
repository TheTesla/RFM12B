
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


void SPI_MasterInit( void);
void SPI_MasterTransmit(char cData);

//Interrupt nach Fertigstellung des ersten SPI-Byte -> zweites senden
ISR(SPI_STC_vect);
//Funktion zum Senden eines 16-Bit Wertes über SPI
void SPI_transfer16(uint16_t spival);


//to do: nebenläufiges Senden von Vektoren
ISR(INT0_vect);
void SPI_join(void);

uint16_t SPI_transfer16_blocking(uint16_t spival);


void rf12_setfreq(unsigned short freq);