#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include "RFM12Bdrv.h"


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
	gSPI_readval_H = SPDR;
	SPI_MasterTransmit(gspival % 256); // letztes Byte
	SPSR = 1 << SPIF; // Interrupt-Flag zurücksetzen
	SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0); // SPI-Interrupt nach Transfer abschalten
	PORT_SPI |= (1 << PORT_SS); //Slave select Ende
	gSPI_ready = 1;
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

//to do: nebenläufiges Senden von Vektoren
ISR(INT0_vect)
{
	SPI_transfer16(0);
}	

void SPI_join(void)
{
	while(0==gSPI_ready); //warten bis zweites Byte an SPI-Schnittstelle übergeben
	while(!(SPSR & (1<<SPIF))); //warten bis dieses Byte die SPI-Schnittstelle verlassen hat
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
