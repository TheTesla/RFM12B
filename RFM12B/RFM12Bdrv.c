#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "RFM12Bdrv.h"

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

#define RF12_INT_ON		GIMSK |= (1 << INT0);
#define RF12_INT_OFF	GIMSK &= ~(1 << INT0);
#define SPI_deselect 	PORT_SPI |= (1 << PORT_SS);
#define SPI_select		PORT_SPI &= ~(1 << PORT_SS);
#define SPI_on			SPCR = (1<<SPIE)|(1<<SPE)|(1<<MSTR)|(0<<SPR1)|(0<<SPR0);
#define SPI_off			SPCR = (1<<SPE)|(1<<MSTR);
#define SPI_ds			SPSR = 1;
#define SPI_ns			SPSR = 0;

volatile uint16_t gspival;
volatile uint8_t gSPI_readval_L;
volatile uint8_t gSPI_readval_H;
volatile uint8_t gSPI_ready;
volatile uint8_t grf12_remaining_bytes_to_transmit;
volatile uint8_t grf12_remaining_bytes_to_receive;
volatile uint8_t *grf12_transmitptr;
volatile uint16_t *grf12_cmdptr;
volatile uint8_t grf12_PM;
volatile uint8_t grf12_TX_running;
volatile uint8_t gi;
volatile uint16_t grf12_cmd;
volatile uint16_t grf12_status1;
volatile uint16_t grf12_status2;
volatile uint8_t grf12_TRXmode;
volatile uint8_t grf12_cmd_tr_mode;
volatile uint8_t grf12_cmd_vec_tr_mode;
volatile uint8_t grf12_remaining_cmds_to_transmit;
volatile uint8_t grf12_read_octet;

volatile uint8_t grf12_frame_state;



volatile void (*grf12_recv_fp) (uint8_t);
volatile void (*grf12_frame_rdy_fp) (void);









uint16_t* RF12config2cmdArray(RF12_config_t RF12config);



void SPI_MasterInit( void)
{
	/* Set MOSI and SCK output, all others input */
	DDR_SPI = (1<<DD_MOSI)|(1<<DD_SCK)|(1<<DD_SS); // SS-Output Softwaregesteuert
	/* Enable SPI, Master, set clock rate fck/16 */
	SPI_on
}
void SPI_MasterTransmit(char cData)
{
	/* Start transmission */
 	SPDR = cData;
}

//Funktion zum Senden eines 16-Bit Wertes über SPI
void SPI_transfer16(uint16_t spival)
{

	SPI_select //Slave select Start
	gspival = spival; 
	SPI_MasterTransmit(gspival >> 8); // erstes Byte
	SPI_MasterInit(); //getauscht

}

void rf12_status_nonblock(void)
{
	SPI_transfer16(0x0000);
}	

void rf12_cmd_transfer(uint16_t cmd)
{
	grf12_cmd_tr_mode = 1;
	gSPI_ready = 0;
	grf12_cmd = cmd;
	rf12_status_nonblock(); // Kommando-Transfer starten
}

void rf12_PM_nonblock(uint8_t CW)
{
	//grf12_cmd_tr_mode = 1; // Kommando an RFM12B senden
	rf12_cmd_transfer(0x8200 | CW);
}	


void rf12_transmitbyte(uint8_t sendbyte)
{
	SPI_transfer16(0xB800 | sendbyte);	
}	 



uint16_t rf12_cmd_transfer_blocking(uint16_t cmd)
{
	grf12_cmd_tr_mode = 1;
	rf12_cmd_transfer(cmd);
	while(gSPI_ready != 6);
	return (gSPI_readval_H << 8) | (gSPI_readval_L);
}	

void rf12_nextbyte(void)
{


	if(0==grf12_remaining_bytes_to_transmit){//schon ausschalten
		if(grf12_frame_ptr){
			grf12_frame_ptr++;
			grf12_transmitptr = grf12_frame_ptr->TXptr;
			grf12_remaining_bytes_to_transmit = grf12_frame_ptr->TXoctets;
			rf12_cmdarray(16, RF12config2cmdArray(grf12_frame_ptr->RF_config));
		}	
	}
	else{
		rf12_transmitbyte(*(grf12_transmitptr++));
	}		
	grf12_remaining_bytes_to_transmit--;

}


void rf12_nextcmd(void)
{

	if(0==grf12_remaining_cmds_to_transmit){
		grf12_cmd_vec_tr_mode = 0; 
		gSPI_ready = 4;
		//frame multiple blocks
		if(grf12_frame_ptr){//Nur, wenn überhaupt frames mit mehreren Blöcken verwendet
			if(grf12_frame_ptr->TXptr){//Wenn frame-Block TX-Daten enthält -> Senden
				grf12_TX_running = 1;
				RF12_INT_ON; // RFM12B-Interrupt immer anschalten, wenn Kommando übertragen ist
				grf12_TRXmode = mode_TX;
				gSPI_ready = 0;
				SPI_transfer16(*grf12_cmdptr++);
			}
			else{
				if(grf12_frame_rdy_fp) grf12_frame_rdy_fp();
			}				
		}			

	}
	else{
		
		SPI_transfer16(*grf12_cmdptr++);
	}		
	grf12_remaining_cmds_to_transmit--;

}

//Interrupt nach Fertigstellung des ersten SPI-Byte -> zweites senden
ISR(SPI_STC_vect)
{
	
	
	if(gSPI_ready % 2){// nach einem vollständigen Befehl
		gSPI_readval_L = SPDR;
		SPI_deselect //Slave select Ende
		SPI_off // SPI-Interrupt nach Transfer abschalten
		//Hier darf RF12_INT_ON nicht stehen, da RFM12 dauernd TX-Interrupt anfordert bis Buffer geladen.
		//gSPI_ready++;		
	}		
	//Um einen Befehl an das RFM12B zu senden, wird rf12_status_nonblock() aufgerufen, das das erste Byte eines Status-Read sendet und diesen SPI-Interrupt aktiviert, um das folgende zweite Byte zu senden: 
	else {// JEWEILS zweites Byte des 2-Byte Kommando (Es gibt ausschließlich 2-Byte Kommandos
		gSPI_readval_H = SPDR;
		SPI_MasterTransmit(gspival % 256); // letztes Byte
		gSPI_ready++;
		return;
	}			
	
	//rf12-command-processing state machine 
	//folgende Bed. sind immer ungerade - werden immer nur ausg., wenn auch Bed. gSPI_ready % 2 ausg. wird
	if(gSPI_ready == 5){
		
		//debug
		//if((1 << 13) & grf12_status2) LED1(1);
		
		grf12_cmd_tr_mode = 0;
		gSPI_ready = 6; // Ende
		RF12_INT_ON; // Nächstes empfangene Octet soll interrupt triggern - per default
		if(grf12_TRXmode == mode_RX){ // Im Empfangsmodus
			if(0xB000 == gspival){ // Vorheriger Befehl muss Lesebefehl gewesen sein - Abfrage erlaubt dauerhaften Empf.modus (grf12_TRXmode = mode_RX) und gleichzeitig andere RF12-Befehle
				// Ja, SPDR kann bereits verarbeitet werden, da Empf.befehl aus nur 1 Byte besteht und 1 Byte Zeropadding hat, wobei Daten beim Einschreiben dieses Bytes bereits ausgelesen werden
				grf12_recv_fp(SPDR); //Empfangenes Octet verarbeiten - Funktion kann auch Empfang beenden
					// -> z. B. 0xCA82 und wichtig: RF12_INT_OFF - damit der RF12-Befehl nicht durch weiteres empfange Octet zwischendrin unt. wird. 
			}
		}
	}

	else if(gSPI_ready == 3){
		grf12_status2 = gSPI_readval_H << 8 | gSPI_readval_L;
		if(grf12_cmd_tr_mode) {
			grf12_cmd_tr_mode = 0;
			SPI_transfer16(grf12_cmd); 
			gSPI_ready = 4; // Kommando fertig übertragen, Ende des SPI-Transfers
		}
		else if(grf12_cmd_vec_tr_mode){ // Liste an Befehlen übertragen
			gSPI_ready = 0;
			rf12_nextcmd();
		}			
		else{// Array-Sende/Empfangs-Modus
			if(mode_TX == grf12_TRXmode){ //Transmit-Mode
				if((1 << 15) & grf12_status2){ // TX-Buffer noch nicht voll
					gSPI_ready = 0; // nächstes Byte transmitten
					rf12_nextbyte();
				}
				else{// TX-Buffer voll
					grf12_cmd_tr_mode = 0;
					RF12_INT_ON;
					gSPI_ready = 4; // Transfer erst mal zuende (Flussregelung)
				}
			}			
			else if(mode_RX == grf12_TRXmode){ //Receive-Mode
				if((1 << 15) & grf12_status2){ //Prüfen, dass es tatsächlich ein Empfangsinterrupt war
					SPI_transfer16(0xB000);
				}				
				gSPI_ready = 4;
																
			}
			else {
				rf12_transmitbyte(0xAA); // Nach Senderabschaltung das Senderegister mit Präambel vorladen
				if((1 << 15) & grf12_status2){ // TX-Buffer noch nicht voll
					gSPI_ready = 0; // nächstes Präambel-Byte transmitten
				}
				else{
					grf12_cmd_tr_mode = 0;
					gSPI_ready = 4; // TX-Buffer voll	
				}										
			}							
		}			
	}		
	// ist das erste Status-Read fertig, gleich noch ein Status-Read starten
	else if(gSPI_ready == 1){
		grf12_status1 = gSPI_readval_H << 8 | gSPI_readval_L;
		rf12_status_nonblock(); 
		gSPI_ready = 2;
	}
	else{
	}		


}



//nebenläufiges Senden von Vektoren
ISR(INT0_vect)
{
	gSPI_ready = 2;
	RF12_INT_OFF; // Interrupt INT0 deaktivieren
	GIFR = (1 << INTF0);
	rf12_status_nonblock();
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
		rf12_cmd_transfer_blocking(0xA000|freq);
}

void rf12_transmitarray(uint8_t n, uint8_t *data)
{
	grf12_TX_running = 1;
	RF12_INT_ON; // RFM12B-Interrupt immer anschalten, wenn Kommando übertragen ist
	grf12_TRXmode = mode_TX;
	grf12_transmitptr = data;
	grf12_remaining_bytes_to_transmit = n;
	gSPI_ready = 0;
	rf12_PM_nonblock(grf12_PM | 0x20); // Transmitter an
}	


void rf12_cmdarray(uint8_t n, uint16_t *cmds)
{
	grf12_cmd_vec_tr_mode = 1;
	//RF12_INT_ON; // RFM12B-Interrupt immer anschalten, wenn Kommando übertragen ist
	grf12_cmdptr = cmds;
	grf12_remaining_cmds_to_transmit = n;
	gSPI_ready = 0;
	rf12_status_nonblock();
}	



uint16_t* RF12config2cmdArray(RF12_config_t RF12config)
{
	
	RF12configcmdarray[0] = 0x8000 | (RF12config.TX_FIFO_en << 7) | (RF12config.RX_FIFO_en << 6) | (RF12config.band << 4) | (RF12config.xtal);
	RF12configcmdarray[1] = 0xCC00 | (RF12config.PLLoutbuf << 5) | (RF12config.xtalLP << 4) | (RF12config.PLLdelay << 3) | (RF12config.DitheringOFF << 2) | (RF12config.PLLbw);
	RF12configcmdarray[2] = 0xC000 | (RF12config.CLKout << 5) | (RF12config.UVdet);
	RF12configcmdarray[3] = 0xA000 | (RF12config.freq);
	RF12configcmdarray[4] = 0xC600 | (RF12config.baudcs << 7) | (RF12config.baudDiv);
	RF12configcmdarray[5] = 0x9000 | (RF12config.pin_nINT_VDI << 10) | (RF12config.VDIr << 8) | (RF12config.DemodBW << 5) | (RF12config.InaG << 3) | (RF12config.rssiTH);
	RF12configcmdarray[6] = 0xCE00 | (RF12config.syncpattern);
	RF12configcmdarray[7] = 0xC200 | (RF12config.autolock << 7) | (RF12config.manuallock_fast << 6) | (1 << 5) | (RF12config.filter_analog << 4) | (1 << 3) | (RF12config.quality_threshold);
	RF12configcmdarray[8] = 0xC400 | (RF12config.automode << 6) | (RF12config.rangelimit << 4) | (RF12config.strobeedge << 3) | (RF12config.finemode << 2) | (RF12config.AFCoe << 1) | (RF12config.AFCen);
	RF12configcmdarray[9] = 0x9800 | (RF12config.polarity << 8) | (RF12config.deviation << 4) | (RF12config.power);
	RF12configcmdarray[10]= 0xE000 | (RF12config.WakeR << 8) | (RF12config.WakeM);
	RF12configcmdarray[11]= 0xC800 | (RF12config.t_aze << 1) | (RF12config.aze_en);
	RF12configcmdarray[12]= 0xCA00 | (RF12config.RX_fifo_INT_lvl << 4) | (RF12config.SyncPat_short << 3) | (RF12config.RX_fifo_allways << 2) | (RF12config.RX_fifo_en << 1) | (RF12config.sensreset_dis);
	RF12configcmdarray[13]= 0x8200 | (RF12config.Recen << 7) | (RF12config.basebanden << 6) | (RF12config.TXen << 5) | (RF12config.Synthen << 4) | (RF12config.Xtalen << 3) | (RF12config.brownouten << 2) | (RF12config.wakeupen << 1) | (RF12config.CLKoutdis);
	RF12configcmdarray[14]= 0xB8AA; // Vorladen des Sendebuffers mit Präambel, vor Sendung ODER füllen nach Senderabschaltung, damit TX-Buffer-empty-Interrupt verschwindet
	RF12configcmdarray[15]= 0xB8AA;
	return RF12configcmdarray;
}	



void rf12_sendframe(rf12_frame_handle_t* rf12_frame)
{
	grf12_frame_ptr = rf12_frame;
	uint8_t k;
	k = 0;
		
	grf12_frameblocks = k;
	
	grf12_transmitptr = grf12_frame_ptr->TXptr;
	grf12_remaining_bytes_to_transmit = grf12_frame_ptr->TXoctets;
	rf12_cmdarray(16, RF12config2cmdArray(grf12_frame_ptr->RF_config));

	
}	




RF12band_t RF12bandCalc(float f)
{
	RF12band_t band;
	band = f_315_MHz;
	if(f > 430) band = f_433_MHz;
	if(f > 860) band = f_868_MHz;
	if(f > 900) band = f_915_MHz;
	return band;	
}	

uint16_t RF12freqCalc(float f)
{
	uint16_t F;
	F = (f - 310.0) * 400.0;
	if(f > 430) F = (f - 430.0) * 400.0;
	if(f > 860) F = (f - 860.0) * 200.0;
	if(f > 900) F = (f - 900.0) * 133.333333;
	return F;
}	

uint8_t RF12bitrateDIV(uint32_t rate)
{
	if(rate >= 2684)	return 10000000 / 29 / rate - 1;
	return 10000000 / 29 / 8 / rate - 1;
}	

unsigned RF12bitrateCS(uint32_t rate)
{
	if(rate >= 2684) return 0;
	return 1;
}	

uint8_t RF12easyBW(uint32_t rate)
{
	if(rate > 115200) return 1;
	if(rate >  57600) return 4;
	if(rate >  19200) return 5;
	return 6;
}	

unsigned RF12easyPLL(uint32_t rate)
{
	if(rate > 57600) return 3;
	return 2;	
}	

uint8_t RF12easyTXdev(uint32_t rate)
{
	if(rate > 115200) return 15;
	if(rate >  57600) return 7;
	if(rate >  19200) return 5;
	return 2;
}	

RF12_config_t RF12_easyconfig(float f, uint32_t rate, uint8_t power, unsigned TX, unsigned RX)
{
	RF12_config_t config;

	config = RF12_default_config;
	config.band =RF12bandCalc(f);
	config.RX_fifo_en = RX;
	config.RX_FIFO_en = 1;
	config.TX_FIFO_en = 1;
	config.TXen = TX;
	config.xtal = c_12pF0;
	config.autolock = 1;
	config.quality_threshold = 2;
	config.freq = RF12freqCalc(f);
	config.Synthen = 1;
	config.DemodBW = RF12easyBW(rate);
	config.baudcs = RF12bitrateCS(rate);
	config.baudDiv = RF12bitrateDIV(rate);
	config.PLLbw = RF12easyPLL(rate);
	config.deviation = RF12easyTXdev(rate);
	config.power = power;
	config.Recen = RX;
	config.basebanden = 1;
	
	return config;
}	

void RF12_init(void)
{
	SPI_ns
	FSK_DDR |= (1 << FSK);
	FSK_PORT |= (1 << FSK);
	SPI_MasterInit();
	MCUCR &= ~(1 << ISC01) & ~(1 << ISC00);  // Low Level Interrupt INT0
	sei();
}	


void rf12_activateRX(void)
{
	grf12_TRXmode = mode_RX;
	RF12_INT_ON; // Damit Empfang möglich ist
}	




