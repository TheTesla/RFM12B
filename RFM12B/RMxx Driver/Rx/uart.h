#ifndef UART_H
#define UART_H
//#define printfoverrs232

#define UART_BAUD_RATE 19200

#define UART_BAUD_CALC(UART_BAUD_RATE,F_CPU) ((F_CPU)/((UART_BAUD_RATE)*16L)-1)

#define UART_BUFFERSIZE 	128			//must be 2^x
#define UART_BUFFERSIZE_MASK	UART_BUFFERSIZE-1

extern volatile unsigned char UART_Rx_Cmd;

void UART_Init(void);
unsigned char UART_Tx_Str(unsigned char *str, unsigned char size);

#ifdef printfoverrs232
int uart_putchar(char c, FILE *stream);
#endif

#endif
