#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>


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

volatile uint16_t RF12configcmdarray[16];

static const uint16_t RXcmdarray[] = {0xCA80, 0xCA82};

void (*grf12_recv_fp) (uint8_t);
void (*grf12_frame_rdy_fp) (void);

#define mode_TX 3
#define mode_RX 2


typedef enum
{
	f_315_MHz = 0,
	f_433_MHz = 1,
	f_868_MHz = 2,
	f_915_MHz = 3
} RF12band_t;	

typedef enum
{
	c_8pF5 = 0,	
	c_9pF0 = 1,	
	c_9pF5 = 2,	
	c_10pF0 = 3,	
	c_10pF5 = 4,	
	c_11pF0 = 5,	
	c_11pF5 = 6,	
	c_12pF0 = 7,	
	c_12pF5 = 8,	
	c_13pF0 = 9,	
	c_13pF5 = 10,	
	c_14pF0 = 11,	
	c_14pF5 = 12,	
	c_15pF0 = 13,	
	c_15pF5 = 14,	
	c_16pF0 = 15	
} RF12xtal_t;	

typedef enum
{
	NORM = 0,
	INV = 1
} RF12mp_t;	

typedef enum
{
	OFF = 0,
	once= 1,
	nVDI= 2,
	ON = 3
} RF12automode_t;

typedef enum
{
	nINT = 0,
	VDI = 1
} RF12pinINTVDI_t;

typedef enum
{
	fast = 0,
	mid = 1,
	slow = 2,
	allwaysON = 3
} RF12vdir_t;


typedef struct
{
	//Grundkonfiguration (Configuration Setting 80xx)
	RF12band_t band;
	unsigned RX_FIFO_en:1;
	unsigned TX_FIFO_en:1;
	RF12xtal_t xtal;
	//Komponentenauswahl (Power Management 82xx)
	unsigned Recen:1;
	unsigned basebanden:1;
	unsigned TXen:1;
	unsigned Synthen:1;
	unsigned Xtalen:1;
	unsigned brownouten:1;
	unsigned wakeupen:1;
	unsigned CLKoutdis:1;

	//Taktgenerator-Einstellungen (PLL Setting CCxx, nur RFM12B)
	unsigned PLLoutbuf:2;
	unsigned xtalLP:1;
	unsigned PLLdelay:1;
	unsigned DitheringOFF:1;
	unsigned PLLbw:2; 

	//Unterspannungs-Detektor und Taktausgangsteiler (LowBatt / µC Clock Control C0xx)
	unsigned CLKout:3;
	unsigned UVdet:5;

	//Frequenzeinstellung (Frequency Setting Axxx)
	unsigned freq:12;

	//Bitrate (Data Rate C6xx)
	unsigned baudcs:1;
	unsigned baudDiv:7;

	//Empfängersteuerung (Receiver Control 9xxx   9000 .. 97FF)
	RF12pinINTVDI_t pin_nINT_VDI;
	RF12vdir_t VDIr;
	unsigned DemodBW:3;
	unsigned InaG:2;
	unsigned rssiTH:3;
	

	//Synchronmuster (Synchron Pattern 0xCExx, nur RFM12B)
	uint8_t syncpattern;

	//Empfangsdatenrekonstruktion (Data Filter C2xx)
	unsigned autolock:1;
	unsigned manuallock_fast:1;
	unsigned filter_analog:1;
	unsigned quality_threshold:3;

	//FIFO-Steuerung (FIFO and RESET Mode CAxx)
	unsigned RX_fifo_INT_lvl:4;
	unsigned SyncPat_short:1;
	unsigned RX_fifo_allways:1;
	unsigned RX_fifo_en:1;
	unsigned sensreset_dis:1;

	//Automatische Frequenznachregelung (Automatic Frequency Control, AFC C4xx)
	RF12automode_t automode;
	unsigned rangelimit:2;
	unsigned strobeedge:1;
	unsigned finemode:1;
	unsigned AFCoe:1;
	unsigned AFCen:1;

	//Senderkonfiguration (TX Configuration 9800 oder 9900)
	RF12mp_t polarity;
	unsigned deviation:4;
	unsigned power:3;

	//Zeitgeber für Wake-Up (Wake-Up Timer Exxx .. Fxxx)
	uint8_t WakeM;
	unsigned WakeR:5;

	//Automatisch zyklischer Empfänger (Low Duty-Cycle C8xx)
	unsigned aze_en:1;
	unsigned t_aze:7;
	
	
} RF12_config_t;





static const RF12_config_t RF12_default_config = 
{
	//Grundkonfiguration (Configuration Setting 80xx)
	.band = 0,
	.RX_FIFO_en = 0,
	.TX_FIFO_en = 0,
	.xtal = 8,
	//Komponentenauswahl (Power Management 82xx)
	.Recen = 0,
	.basebanden = 0,
	.TXen = 0,
	.Synthen = 0,
	.Xtalen = 1,
	.brownouten = 0,
	.wakeupen = 0,
	.CLKoutdis = 0,

	//Taktgenerator-Einstellungen (PLL Setting CCxx, nur RFM12B)
	.PLLoutbuf = 3,
	.xtalLP = 1,
	.PLLdelay = 0,
	.DitheringOFF = 1,
	.PLLbw = 3, 

	//Unterspannungs-Detektor und Taktausgangsteiler (LowBatt / µC Clock Control C0xx)
	.CLKout = 0,
	.UVdet = 0,

	//Frequenzeinstellung (Frequency Setting Axxx)
	.freq = 1664,

	//Bitrate (Data Rate C6xx)
	.baudcs = 0,
	.baudDiv = 35,

	//Empfängersteuerung (Receiver Control 9xxx   9000 .. 97FF)
	.pin_nINT_VDI = 0,
	.VDIr = 0,
	.DemodBW = 4,
	.InaG = 0,
	.rssiTH = 0,
	

	//Synchronmuster (Synchron Pattern 0xCExx, nur RFM12B)
	.syncpattern = 0xD4,

	//Empfangsdatenrekonstruktion (Data Filter C2xx)
	.autolock = 0,
	.manuallock_fast = 0,
	.filter_analog = 0,
	.quality_threshold = 4,

	//FIFO-Steuerung (FIFO and RESET Mode CAxx)
	.RX_fifo_INT_lvl = 8,
	.SyncPat_short = 0,
	.RX_fifo_allways = 0,
	.RX_fifo_en = 0,
	.sensreset_dis = 0,

	//Automatische Frequenznachregelung (Automatic Frequency Control, AFC C4xx)
	.automode = 3,
	.rangelimit = 3,
	.strobeedge = 0,
	.finemode = 1,
	.AFCoe = 1,
	.AFCen = 1,

	//Senderkonfiguration (TX Configuration 9800 oder 9900)
	.polarity = 0,
	.deviation = 0,
	.power = 0,

	//Zeitgeber für Wake-Up (Wake-Up Timer Exxx .. Fxxx)
	.WakeR = 1,
	.WakeM = 0x96,

	//Automatisch zyklischer Empfänger (Low Duty-Cycle C8xx)
	.t_aze = 7,
	.aze_en = 0
};


typedef struct
{
	RF12_config_t RF_config;
	uint8_t* TXptr;
	uint8_t TXoctets;
} rf12_frame_handle_t;


volatile rf12_frame_handle_t* grf12_frame_ptr;
volatile uint8_t grf12_frameblocks;


volatile RF12_config_t grf12_nxtconfig;

uint16_t* RF12config2cmdArray(RF12_config_t RF12config);



void SPI_MasterInit(void);
void SPI_MasterTransmit(char cData);
void SPI_transfer16(uint16_t spival);

void rf12_status_nonblock(void);

void rf12_cmd_transfer(uint16_t cmd);
void rf12_PM_nonblock(uint8_t CW);

void rf12_transmitbyte(uint8_t sendbyte);


uint16_t rf12_cmd_transfer_blocking(uint16_t cmd);

void rf12_nextbyte(void);

void rf12_nextcmd(void);

void SPI_join(void);

uint16_t SPI_transfer16_blocking(uint16_t spival);

void rf12_setfreq(unsigned short freq);

void rf12_transmitarray(uint8_t n, uint8_t *data);

void rf12_cmdarray(uint8_t n, uint16_t *cmds);
uint16_t* RF12config2cmdArray(RF12_config_t RF12config);

void rf12_sendframe(rf12_frame_handle_t* rf12_frame);




RF12band_t RF12bandCalc(float f);

uint16_t RF12freqCalc(float f);

uint8_t RF12bitrateDIV(uint32_t rate);

unsigned RF12bitrateCS(uint32_t rate);

uint8_t RF12easyBW(uint32_t rate);

unsigned RF12easyPLL(uint32_t rate);
uint8_t RF12easyTXdev(uint32_t rate);

RF12_config_t RF12_easyconfig(float f, uint32_t rate, uint8_t power, unsigned TX, unsigned RX);
void RF12_init(void);
void rf12_activateRX(void);




