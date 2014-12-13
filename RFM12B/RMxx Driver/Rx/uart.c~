#ifdef printfoverrs232
	#include <stdio.h>
#endif
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "uart.h"
#
unsigned char UART_Rx_Buffer[UART_BUFFERSIZE];
volatile unsigned char UART_Rx_Index;
unsigned char UART_Tx_Buffer[UART_BUFFERSIZE];
unsigned char UART_Tx_Ring_txptr;
unsigned char UART_Tx_Ring_rxptr;
volatile unsigned char UART_Tx_Ring_Size;

volatile unsigned char UART_Rx_Cmd;
unsigned char UART_Buffer[64];

SIGNAL(SIG_UART_RECV)
{
	if(UART_Rx_Index >= UART_BUFFERSIZE)
		UART_Rx_Index = 0;
	UART_Rx_Buffer[UART_Rx_Index] = UDR;

	if(UART_Rx_Buffer[UART_Rx_Index++] == '\n')
	{
		UART_Rx_Cmd = 1;
	}
}

SIGNAL(SIG_UART_DATA)
{
	UDR = UART_Tx_Buffer[UART_Tx_Ring_rxptr++];
	UART_Tx_Ring_rxptr &= UART_BUFFERSIZE_MASK;
	UART_Tx_Ring_Size--;
	if(UART_Tx_Ring_Size == 0)
		UCSRB &= ~(1<<UDRIE);
}


#ifdef printfoverrs232
static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,_FDEV_SETUP_WRITE);

int uart_putchar(char c, FILE *stream)
{
	loop_until_bit_is_set(UCSRA, UDRE);
	UDR = c;
	return 0;
}
#endif


unsigned char UART_Tx_Str(unsigned char *str, unsigned char size)
{
	unsigned char Tx_Str_loop;
	if(size >= UART_BUFFERSIZE)
		return(2);
	if(UART_Tx_Ring_Size + size >= UART_BUFFERSIZE)
		return(1);
	for(Tx_Str_loop=0; Tx_Str_loop<size; Tx_Str_loop++)
	{
		UART_Tx_Buffer[UART_Tx_Ring_txptr++] = str[Tx_Str_loop];
		UART_Tx_Ring_txptr &= UART_BUFFERSIZE_MASK;
	}
	cli();
	UART_Tx_Ring_Size += size;
	UCSRB |= (1<<UDRIE);
	sei();
	return(0);
}

void UART_Init(void)
{

	UCSRB |= (1<<TXEN)|(1<<RXEN)|(1<<RXCIE);
	UCSRC |= (1<<URSEL)|(3<<UCSZ0);
	UBRRH=(uint8_t)(UART_BAUD_CALC(UART_BAUD_RATE,F_CPU)>>8);
	UBRRL=(uint8_t)UART_BAUD_CALC(UART_BAUD_RATE,F_CPU);

	UART_Rx_Cmd = 0;
	UART_Rx_Index = 0;
	UART_Tx_Ring_txptr = 0;
	UART_Tx_Ring_rxptr = 0;
	UART_Tx_Ring_Size = 0;
 
#ifdef printfoverrs232
	stdout = &mystdout;
#endif

}
