/***************************************************************************************
 * ��������� ������ � 4-� ���. �� ������ ������ ����� �� �������� DMA ��� SPI
 * ������� ������� ������� �������
 *************************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "circbuf.h"
#include "sintab.h"
#include "ads1282.h"
#include "timer3.h"
#include "xpander.h"
#include "eeprom.h"
#include "timer0.h"
#include "utils.h"
#include "main.h"
#include "ports.h"
#include "irq.h"
#include "dac.h"
#include "spi0.h"
#include "pll.h"
#include "log.h"

/**
 * ���������  
 */
#define			PING_PONG_SIZE			1000	/* ������ ������ ������  */
#define  		TEST_BUFFER_SIZE 		100	/* ������� */
#define			BYTES_IN_DATA			3	/* ����� ���� � ����� ������ ��� */

/**
 * ������ ��������� 
 */
#define 	ID		0x00
#define 	CONFIG0 	0x01
#define 	CONFIG1 	0x02
#define 	HPF0	 	0x03
#define 	HPF1	 	0x04
#define 	OFC0	 	0x05
#define 	OFC1	 	0x06
#define 	OFC2	 	0x07
#define 	FSC0	 	0x08
#define 	FSC1	 	0x09
#define 	FSC2	 	0x0a

/************************************************************************
 * ADS1282 commands - ����������� �� ��������
 ************************************************************************/
#define 	WAKEUP		0x01
#define 	STANDBY	    	0x02
#define 	ADC_SYNC	0x04
#define 	RESET		0x06
#define 	RDATAC		0x10	/* ����������� ������ */
#define 	SDATAC		0x11	/* ���� ����������� ������  */
#define 	RDATA		0x12	/* ������ �� ������� */
#define 	OFSCAL		0x60	/* �������� ���� */
#define 	GANCAL		0x61	/* ����������� */
/***********************************************************************************
 * ���� ping_pong_flag == 0 - �������� ����, ���������� �� ����� ����
 * ���� ping_pong_flag == 1 - �������� �o��, ���������� �� ����� ����
 * ������ ������ PING ��� PONG ������ ��� ������ 4!
 * ������ ������ PING ��� PONG - ������� �����:
 ***********************************************************************************/
section("FLASH_data")
static const ADS1282_WORK_STRUCT adc_work_struct[] = {
// ����� ���� � ����� ������ ���������
// ����� ������ ������ ping
// ����� �������� �� ���� ������� � ����� ping - �� ������ ��������� 12000
// ������ � ������ ������ ����
    //freq, bytes, sec, num_in_pack, pack_size, period_us, sig in min, sig in hour
    {SPS62,  3, 60, 3750, 11250, 16000, 1, 60},	// 62.5 �� 3  ����. � ������. 60 ���
    {SPS62,  6, 30, 1875, 11250, 16000, 2, 120},	// 62.5 �� 6  ����. � ������. 30 ���
    {SPS62,  9, 20, 1250, 11250, 16000, 3, 180},	// 62.5 �� 9  ���� � ������. 20 ���
    {SPS62, 12, 12,  750,  9000, 16000, 5, 300},	// 62.5 �� 12 ���� � ������. 12 ���

    {SPS125,  3, 20, 2500,  7500, 8000,  3, 180},	// 125 �� 3  ����. � ������. 20 ���
    {SPS125,  6, 12, 1500,  9000, 8000,  5, 300},	// 125 �� 6  ����. � ������. 12 ���
    {SPS125,  9, 10, 1250, 11250, 8000,  6, 360},	// 125 �� 9  ���� � ������. 10 ���
    {SPS125, 12,  6,  750,  9000, 8000, 10, 600},	// 125 �� 12 ���� � ������. 6 ���

    {SPS250,  3, 12, 3000,  9000, 4000,  5, 300},	// 250 �� 3  ����. � ������. 12 ���
    {SPS250,  6,  6, 1500,  9000, 4000, 10, 600},	// 250 �� 6  ����. � ������. 6 ���
    {SPS250,  9,  4, 1000,  9000, 4000, 15, 900},	// 250 �� 9  ���� � ������. 4 ���
    {SPS250, 12,  4, 1000, 12000, 4000, 15, 900},	// 250 �� 12 ���� � ������. 4 ���

    {SPS500,  3, 6, 3000,  9000, 2000, 10,  600},	// 500 �� 3  ���� � ������. 6 ���
    {SPS500,  6, 4, 2000, 12000, 2000, 15,  900},	// 500 �� 6  ���� � ������. 4 ���
    {SPS500,  9, 2, 1000,  9000, 2000, 30, 1800},	// 500 �� 9  ���� � ������  2 ���
    {SPS500, 12, 2, 1000, 12000, 2000, 30, 1800},	// 500 �� 12 ���� � ������. 2 ���

    {SPS1K,  3, 4, 4000, 12000, 1000, 15,  900},	// 1000 �� 3  ���� � ������. 4 ���
    {SPS1K,  6, 2, 2000, 12000, 1000, 30, 1800},	// 1000 �� 6  ���� � ������. 2 ���
    {SPS1K,  9, 1, 1000,  9000, 1000, 60, 3600},	// 1000 �� 9  ���� � ������. 1 ���
    {SPS1K, 12, 1, 1000, 12000, 1000, 60, 3600},	// 1000 �� 12 ���� � ������. 1 ���

    {SPS2K,  3, 2, 4000, 12000, 500,  30, 1800},	// 2000 �� 3 ����. � ������. 2 ���
    {SPS2K,  6, 1, 2000, 12000, 500,  60, 3600},	// 2000 �� 6 ����. � ������. 1 ���
    {SPS2K,  9, 0, 1000,  9000, 500, 120, 7200},	// 2000 �� 9  ���� � ������. 1/2 ��� 
    {SPS2K, 12, 0, 1000, 12000, 500, 120, 7200},	// 2000 �� 12 ���� � ������. 1/2 ��� 

    {SPS4K,  3, 1, 4000, 12000, 250,  60,  3600},	// 4000 �� 3 ����. � ������. 1 ���
    {SPS4K,  6, 0, 2000, 12000, 250, 120,  7200},	// 4000 �� 6 ����. � ������. 1/2 ���
    {SPS4K,  9, 0, 1000,  9000, 250, 240, 14400},	// 4000 �� 9  ���� � ������. 1/4 ���
    {SPS4K, 12, 0, 1000, 12000, 250, 240, 14400},	// 4000 �� 12 ���� � ������. 1/4 ���
};

/**
 * ��������� ���������� ������� ��� ��� � ����� ������ ���. ��������� ��� ���������� 
 * ��������� ���������
 */
static struct {
	ADS1282_Regs regs;	/*  ������������ ��� */

	u64 long_time0;
	u64 long_time1;

	u32 test_irq_num;
	u32 num_irq;		/* ���������� ���������� */
	s32 num_sig;		/* ����� ������� */
	u32 sig_in_time;	/* ��� ����� ���������� ������� ���� */
	u16 pack_cnt;		/* ������� ������� */
	u16 sig_in_min;		/* ��� ����� ���������� ��������� */

	u16 ping_pong_size;	/* ����� ��������� ������� �� �������� ����� */
	u16 sample_us;		/* ���� ������� ���������� ���������� � ������������� */

	u8 sps_code;		/* ��� ������� ������������� */
	u8 data_size;		/* ������ ������ ������ �� ���� ������� 3..6..9..12 */
	u8 bitmap;		/* ����� ������� */
	u8 ping_pong_flag;	/* ���� ������ - ���� ��� ����  */
	u8 mode;		/* ����� ������ - test, work ��� cmd */

	bool handler_write_flag;	/* ���� ����, ��� ������ ������� */
	bool handler_sig_flag;	/* ��� �������� */
	bool is_set;		/* ���������� */
	bool is_init;		/* ���������� ��� "���������������" */
	bool is_run;		/* ���������� ��� "��������" */
} adc_pars_struct;

/**
 * ������ ��� ����� ����
 */
static ADS1282_ERROR_STRUCT adc_error;

static ADS1282_PACK_STRUCT *pack;
static CircularBuffer cb;

static u8 *ADC_DATA_ping = 0;
static u8 *ADC_DATA_pong = 0;


/*************************************************************************************
 * 	����������� ������� - ����� ���� �����
 *************************************************************************************/
static bool save_adc_const(u8, u32, u32);
static void pin_config(void);
static void irq_register(void);
static void cmd_write(u8);
static void reg_write(u8, u8);
static u8 reg_read(u8);
static int regs_config(u8, u8, u16);
static bool write_adc_const(u8, u32, u32);
static bool read_adc_const(u8, u32 *, u32 *);
static void adc_reset(void);
static void signal_handler(int);
static inline void adc_irq_acc(void);
static inline void adc_sync(void);

static void select_chan(int);
static void read_data_from_spi(int, u32[]);
static void cmd_mode_handler(u32 *, int);
static void work_mode_handler(u32 *, int);

/**
 * ��������� � ��������� ������ ������ ��� ������� � ����� ���� � ������
 * ��� 125 � 62.5 ����� ������� ������� ����������� ���
 */
#pragma section("FLASH_code")
static int calculate_ping_pong_buffer(ADS1282_FreqEn freq, u8 bitmap, int len)
{
	int i;
	int res = -1;
	u8 bytes = 0;


	/* ����� ������ �����. ������ 1 ������ (����� 1 * 3 - �� ���� ���������� ��� �����) */
	for (i = 0; i < 4; i++) {
		if ((bitmap >> i) & 0x01)
			bytes += 3;
	}

	/* ����� ����� ���������� ������ ������ ping-pong */
	for (i = 0; i < sizeof(adc_work_struct) / sizeof(ADS1282_WORK_STRUCT); i++) {

		if ( /*freq == 0 || */ bytes == 0 || len == 0) {
			break;
		}

		if (freq == adc_work_struct[i].sample_freq && bytes == adc_work_struct[i].num_bytes) {

			adc_pars_struct.bitmap = bitmap;	/* ����� ������ �����  */
			adc_pars_struct.data_size = adc_work_struct[i].num_bytes;
			adc_pars_struct.ping_pong_size = adc_work_struct[i].samples_in_ping;	/* ����� �������� � ����� ping */
			adc_pars_struct.sig_in_min = adc_work_struct[i].sig_in_min;	/* �������� signal ��� � ������ */
			adc_pars_struct.sig_in_time = adc_work_struct[i].sig_in_time;	/* ��������� ������� ������� ��� �� 1 ��� */
			adc_pars_struct.sig_in_time *= len;	/* ������� ����� �����? **** */

			adc_pars_struct.num_sig = 0;
			adc_pars_struct.num_irq = 0;
			adc_pars_struct.ping_pong_flag = 0;
			adc_pars_struct.handler_sig_flag = false;
			adc_pars_struct.pack_cnt = 0;	/* �������� �������� */
			adc_pars_struct.sample_us = adc_work_struct[i].period_us;

			/* ������� ������ ��� ������ - ��� ������ 1 � 2 �, 4 � ��� ������ */
			if (ADC_DATA_ping == NULL && ADC_DATA_pong == NULL) {
				ADC_DATA_ping = calloc(adc_work_struct[i].ping_pong_size_in_byte, 1);
				ADC_DATA_pong = calloc(adc_work_struct[i].ping_pong_size_in_byte, 1);

				if (ADC_DATA_ping == NULL || ADC_DATA_pong == NULL) {
					log_write_log_file("ERROR: Can't alloc memory for ping-pong\n");
					break;
				}
				log_write_log_file("INFO: freq:  %d Hz, bytes: %d\n",
						   TIMER_US_DIVIDER / adc_pars_struct.sample_us, bytes);
				log_write_log_file("INFO: alloc: %d bytes for ping & pong\n",
						   adc_work_struct[i].ping_pong_size_in_byte * 2);
			}
			res = 0;
			break;
		}
	}
	/* ���� - ����. 5% */
	adc_pars_struct.sample_us += adc_pars_struct.sample_us / 20;
	return res;
}


/**
 * ���������������� ��� ��� �������
 * ������� ��� � ��������� - ������� ����������� ������
 * ��������� ������������ ����������, ������� ���� ��������
 */
#pragma section("FLASH_code")
bool ADS1282_config(void *arg)
{
	int volatile i;
	ADS1282_Params *par;
	u8 cfg1, cfg0, hi = 0;
	u16 hpf;
	bool res = false;

	do {
		if (arg == NULL) {
			break;
		}

		IRQ_unregister_vector(ADS1282_VECTOR_NUM);	/* ���� ������� - ��������� ���������� ��� */

		par = (ADS1282_Params *) arg;
		adc_pars_struct.mode = par->mode;	/* ��������� ����� ������ */
		read_ads1282_coefs_from_eeprom(&adc_pars_struct.regs);	/* �������� ������������ �� eeprom - ��� ������� ���� �������� ���������� ������������ */

		hi = (par->res == 1) ? 1 : 0;	/* ����������������� */
		cfg0 = (hi << 6) | (1 << 1);	/* ������ �������: Pulse SYNC, Sample frequency = 250 Hz, High-resolution, Linear phase, Sinc + LPF filter blocks */

		/* ���� �������� ���������. NB: SIVY �� ������������ ������� ������ 4 ��� 
		 * ���������� ������� � fpga � ������� �� ������ */
		if (par->sps > SPS4K || par->pga > 6) {
			log_write_log_file("ERROR: ADC doesn't support this parameters! freq = %d. pga = %d\n",
					   (1 << par->sps) * 125 / 2, 1 << par->pga);
			break;
		}
		log_write_log_file("INFO: ADC parameters OK! freq = %d. pga = %d\n", (1 << par->sps) * 125 / 2,
				   1 << par->pga);


/* �� �������� ����� 250 ��������� ������� ������������
 * ���� 125 - � 2 ����
 * ���� 62.5 - � 4 ���� 
 */
		adc_pars_struct.sps_code = par->sps;

		/* ����� ������������ � ������� ������ */
		switch (par->sps) {
		case SPS4K:
			cfg0 |= 4 << 3;
			break;

		case SPS2K:
			cfg0 |= 3 << 3;
			break;

		case SPS1K:
			cfg0 |= 2 << 3;
			break;

		case SPS500:
			cfg0 |= 1 << 3;
			break;

		case SPS250:
			cfg0 |= 1 << 3;
			break;

#if QUARTZ_CLK_FREQ==(8192000)
		case SPS125:
			cfg0 |= 0 << 3;
			TIMER3_change_freq(SPS125);
			break;

		case SPS62:		
		default:
			cfg0 |= 0 << 3;
			TIMER3_change_freq(SPS62);
			break;
#endif		
		}


//////

		cfg1 = (par->pga);	/* ������ �������: PGA chopping disabled + PGA Gain Select. AINP1 � AINN1 */


		/* ������� chopping  */
		if (par->chop) {
			cfg1 |= (1 << 3);
		}

		hpf = par->hpf;	/* ���� (-1) ��� (0) �� ������ ������ � �� ������ �����������-����� �������������� */
		if (hpf == 0xffff || hpf == 0) {
			log_write_log_file("WARN: HI-Pass filter is not correct (0x%04X)\n", hpf);
			log_write_log_file("WARN: Filter won't be set\n");
			hpf = 0;
		}

		/* � ������� ������ �������� ��� ���������  */
		if (par->mode == WORK_MODE) {

			if (calculate_ping_pong_buffer
			    ((ADS1282_FreqEn) adc_pars_struct.sps_code, par->bitmap, par->file_len) != 0) {
				log_write_log_file("ERROR: Can't calculate buffer PING PONG\n");
				break;
			}

			/* ������ ��������� ��������� */
			log_fill_adc_header(adc_pars_struct.sps_code, adc_pars_struct.bitmap,
					    adc_pars_struct.data_size);
			log_write_log_file("INFO: change ADS1282 header...OK\n");

		} else if (par->mode == CMD_MODE) {

			/* ���� ��� ������ ��� ������ - �������� ��� */
			if (pack == NULL) {
				pack = calloc(sizeof(ADS1282_PACK_STRUCT), 1);
				if (pack == NULL) {
					adc_pars_struct.is_run = false;
					break;
				}
			}
			pack->rsvd = 0;	/* ������ ��� �������� */

			/* ���� ����� �� ���������� �������� ��� �� TEST_BUFFER_SIZE ����� ADS1282_PACK_STRUCT */
			if (!adc_pars_struct.is_run && cb_init(&cb, TEST_BUFFER_SIZE)) {
				adc_pars_struct.is_run = false;
				break;
			}


		} else {	/* ��st mode */
			log_write_log_file("--------------------Checking ADS1282 config-----------------\n");

			/* ������ �������: Pulse SYNC, Sample frequency = 250 Hz, High-resolution, Linear phase, Sinc + LPF filter blocks */
			cfg0 = (hi << 6) | (1 << 1);
			cfg0 |= ((par->sps - 1) << 3);
			cfg1 = (1 << 3) | 2;
		}
		memset(&adc_error, 0, sizeof(adc_error));

		adc_pars_struct.is_set = true;
		if (regs_config(cfg0, cfg1, hpf) == 0) {	/*  ������������� �������� ��� �� ���� ������� */
			adc_pars_struct.is_init = true;	/* ���������������  */
			res = true;
		} else {
			log_write_log_file("Error config regs\n");
			adc_pars_struct.is_init = false;	/* ���������������  */
		}
	} while (0);
	return res;
}


/**
 * ��������������� �������� ���� 4 ���, SPI0, ��������� ������� � ���� �������
 * ����������� ������ _1282_OWFL.._1282_DRDY - ����, F14..CH_SEL_B - �����
 * ����� �������� Data Rate � ���������
 */
#pragma section("FLASH_code")
static int regs_config(u8 cfg0, u8 cfg1, u16 hpf)
{
	int i;
	int res = 0;

	pin_config();		/* ������ DRDY � OWFL + ������������� + SPI0 */
	adc_reset();		/* Before using the Continuous command or configured in read Data serial interface: reset the serial interface. */

	if (!adc_pars_struct.is_set)
		return -1;

	/* �������� ���, � ����������� �� ����� ���������� ������� */
	for (i = 0; i < ADC_CHAN; i++) {
		u8 byte0, byte1, byte2;
		u16 temp;

		select_chan(i);

		/* ���������. The SDATAC command must be sent be read by Read Data By Command. 
		 * The Read before register read/write operations to cancel the Data opcode
		 * command must be sent in this mode Read Data Continuous mode  
		 */
		cmd_write(SDATAC);

		/* ������ �������, ���� ����� ���� �� ��������� ��-��������� */
		if (hpf != 0) {
			cfg0 |= 0x03;	/* HPF + LPF + Sinc filter  */
			reg_write(HPF0, hpf);	/* ������� */
			reg_write(HPF1, hpf >> 8);	/* �������  */
		}
		reg_write(CONFIG0, cfg0);	/* SPS = xxx  */
		reg_write(CONFIG1, cfg1);	/* PGA = xxx , AINP1 + AINN1 +  � ������� */

		/* � �������� �������� ������� ������ - ������ ��������� �� �� ����� ��� �������� */
		log_write_log_file("------------------ADC %d tunning------------------\n", i + 1);
		log_write_log_file("ID  : 0x%02x\n", reg_read(ID));


		byte0 = reg_read(CONFIG0);
		if (byte0 != cfg0) {
			log_write_log_file("FAIL: CFG0 = 0x%02x\n", byte0);
			res = -1;
		}
		log_write_log_file("CFG0: 0x%02x\n", byte0);

		adc_error.adc[i].cfg0_wr = cfg0;
		adc_error.adc[i].cfg0_rd = byte0;


		byte1 = reg_read(CONFIG1);
		if (byte1 != cfg1) {
			log_write_log_file("FAIL: CFG1 = 0x%02x\n", byte1);
			res = -1;
		}
		log_write_log_file("CFG1: 0x%02x\n", byte1);

		adc_error.adc[i].cfg1_wr = cfg1;
		adc_error.adc[i].cfg1_rd = byte1;


		if (hpf != 0) {
			byte0 = reg_read(HPF0);
			byte1 = reg_read(HPF1);
			temp = ((u16) byte1 << 8) | byte0;

			if (temp != hpf) {
				log_write_log_file("FAIL: HPF = 0x%04x. Need to be 0x%04x\n", temp, hpf);
			}
			log_write_log_file("HPF : 0x%04x\n", hpf);
		}

		/* ��� ������� EEPRO� ��� ���� ��� ������� */
		if (hpf == 0) {
			log_write_log_file("Get from EEPROM OFFSET: %d and GAIN: %d\n",
					   adc_pars_struct.regs.chan[i].offset, adc_pars_struct.regs.chan[i].gain);

			/* �������� - �������� ����� � EEPROM ��� ���� ����� */
			if (abs((int) adc_pars_struct.regs.chan[i].offset) > 100000) {
				log_write_log_file("Offset error! set to 64000\n");
				adc_pars_struct.regs.chan[i].offset = 64000;
			}
		} else {
			log_write_log_file("Set offset to 0\n");
			adc_pars_struct.regs.chan[i].offset = 0;
		}

		/* ��������� ������  eeprom */
		if (abs((int) adc_pars_struct.regs.chan[i].gain) < 2000000
		    && abs((int) adc_pars_struct.regs.chan[i].gain) > 8000000) {
			log_write_log_file("Gain error! set to 0x400000\n");
			/* ���� ������ - offset ������ gain! */
			adc_pars_struct.regs.chan[i].gain = 0x400000;
		}

		/* ������� � ������� ��� */
		if (write_adc_const(i, adc_pars_struct.regs.chan[i].offset, adc_pars_struct.regs.chan[i].gain) == true) {	/* ������� �� ������ ��������� */
			log_write_log_file("Write regs offset: %d & gain: %d OK\n", adc_pars_struct.regs.chan[i].offset,
					   adc_pars_struct.regs.chan[i].gain);
		} else {
			log_write_log_file(" FAIL: Write regs offset & gain\n");
			res = -1;
		}
	}
	log_write_log_file("-------------- end reg_config ---------------\n");

	return res;
}


/**
 * ��������� ��� ��� � �������� �������������
 */
#pragma section("FLASH_code")
void ADS1282_start(void)
{
	u8 i;
	u8 cfg1;

	if (adc_pars_struct.is_run) {	/* ��� ��������� */
		return;
	}


	/* ������ ������ �� IVG14 ���� � ������� ������. �� pga �� �������. */
	if (adc_pars_struct.mode == WORK_MODE) {
		adc_pars_struct.handler_write_flag = true;	/* � ������ ��� �������, ��� ����� ������ */
		signal(SIGIVG14, signal_handler);
	}

	/* Set the data mode. ��������� ��� */
	for (i = 0; i < ADC_CHAN; i++) {
		select_chan(i);
		cmd_write(RDATAC);
	}

	adc_sync();		/* �������������� ��� */
	IRQ_register_vector(ADS1282_VECTOR_NUM);	/* ������������ ���������� ��� ��� �� 8 ��������� */
	irq_register();		/* ������ ������������� IRQ */
	select_chan(0);		/* �������� 0-� ����� */
	adc_pars_struct.num_irq = 0;
	adc_pars_struct.is_run = true;	/* ��� ��������� */
	log_write_log_file("INFO: ADS1282 start OK\n");
}


/**
 * ���� ��� �� ������ CONTINIOUS � PowerDown, �������� �������
 * ���������� SPI0
 */
#pragma section("FLASH_code")
void ADS1282_stop(void)
{
	int i;

	ADS1282_stop_irq();	/* ��������� ���������� */
	IRQ_unregister_vector(ADS1282_VECTOR_NUM);	/* ���� ������� - ��������� ���������� ��� */
	pin_config();		/* ������ DRDY � OWFL + ������������� + SPI0 */

	for (i = 0; i < ADC_CHAN; i++) {
		select_chan(i);	/* �������� ��� */
		cmd_write(SDATAC);

		if (WORK_MODE == adc_pars_struct.mode) {
			u16 hpf;
			log_write_log_file("------------------ADC stop------------------\n");
			log_write_log_file("CFG0: 0x%02x\n", reg_read(CONFIG0));	// ��������� �������� ��� ��������
			log_write_log_file("CFG1: 0x%02x\n", reg_read(CONFIG1));

			hpf = ((u16) reg_read(HPF1) << 8) | reg_read(HPF0);
			log_write_log_file("HPF : 0x%04x\n", hpf);
			cmd_write(STANDBY);
		}
	}


	/* ������� �����  */
	if (WORK_MODE == adc_pars_struct.mode) {

		/* ������� ������ */
		if (ADC_DATA_ping != NULL) {
			free(ADC_DATA_ping);
			ADC_DATA_ping = 0;
		}

		if (ADC_DATA_pong != NULL) {
			free(ADC_DATA_pong);
			ADC_DATA_pong = 0;
		}
	}
	adc_pars_struct.is_run = false;	/* �� �������  */
	adc_pars_struct.is_init = false;	/* ��������� �������������  */
	adc_pars_struct.num_sig = 0;
	adc_pars_struct.test_irq_num = adc_pars_struct.num_irq;	/* �������� ����� */
	adc_pars_struct.num_irq = 0;
	adc_pars_struct.pack_cnt = 0;	/* �������� �������� */
	adc_pars_struct.ping_pong_flag = 0;
	adc_pars_struct.handler_sig_flag = false;

	SPI0_stop();
}

/**
 * ���������� � ��������� ������ - ����� �� FLASH
 * ����� ������ � Little Endian
 */
#pragma section("L1_code")
static void cmd_mode_handler(u32 * data, int num)
{
	DEV_STATUS_STRUCT status;

	if (adc_pars_struct.pack_cnt == 0) {
		u64 time_ms = get_msec_ticks();

		/* ������� + ������������ ������� ������ */
		pack->adc = adc_pars_struct.sps_code;	/* ��� ������� ������������� */
		pack->msec = time_ms % 1000;	/* ������������ ������� ������  */
		pack->sec = time_ms / 1000;	/* ����� UNIX                   */
	}

	/* ������� ����� ������� ���� */
	pack->data[adc_pars_struct.pack_cnt].x = data[0] >> 8;
	pack->data[adc_pars_struct.pack_cnt].y = data[1] >> 8;
	pack->data[adc_pars_struct.pack_cnt].z = data[2] >> 8;
	pack->data[adc_pars_struct.pack_cnt].h = data[3] >> 8;
	adc_pars_struct.pack_cnt++;

	/* ����� 0...19 � �������� �����  */
	if (adc_pars_struct.pack_cnt >= NUM_ADS1282_PACK) {
		adc_pars_struct.pack_cnt = 0;
		if (cb_is_full(&cb)) {
			cmd_get_dsp_status(&status);
			status.st_main |= 0x20;	/* ������ ������ - "����� �����". ������� ��� ������, �� �����! */
			pack->rsvd++;
			cmd_set_dsp_status(&status);
		}
		cb_write(&cb, pack);	/* ����� � ����� */
	}
}



/**
 * ���������� � ������� ������ 4 ������ ������
 */
section("L1_code")
static void work_mode_handler(u32 * data, int num)
{
	u8 *ptr;
	register u8 i, ch, shift = 0;
	u32 d;
	u64 ns, us;

	ns = get_long_time();	// ����� � ������������
	us = ns / 1000;		// ������������
	adc_error.time_last = adc_error.time_now;
	adc_error.time_now = us;	// ����� ������

	/* ������� ������� ���������� ���������� ���. ���� �������� �� 5% - ����������� ������ ��� */
	if (adc_error.time_last != 0 && adc_error.time_now - adc_error.time_last > adc_pars_struct.sample_us) {
		adc_error.sample_miss++;
	}


	/* ����� ����� ������� ������ */
	if (0 == adc_pars_struct.pack_cnt) {
		adc_pars_struct.long_time0 = ns;	/* ������� + ����������� ������� ������ */
	}

	/* �������� ���� ��� ���� �� 1 ������� */
	if (0 == adc_pars_struct.ping_pong_flag) {
		ptr = ADC_DATA_ping;	/* 0 �������� ���� */
	} else {
		ptr = ADC_DATA_pong;	/* 1 �������� ���� */
	}


	/* ����� ������ �������� */
	ch = adc_pars_struct.bitmap;

	for (i = 0; i < ADC_CHAN; i++) {
		if ((1 << i) & ch) {
			d = byteswap4(data[i]);	/* ���� ����� ������� - ���������� � Big Endian � ������� ������ 3 ����� */
			memcpy(ptr + adc_pars_struct.pack_cnt * adc_pars_struct.data_size + shift, &d, BYTES_IN_DATA);
			shift += BYTES_IN_DATA;
		}
	}

	adc_pars_struct.pack_cnt++;	/* ������� ������� ��������  */

	/*  1 ��� � ������� �� 2 ��� �� 4 ������� ����� 1000 ������� */
	if (adc_pars_struct.pack_cnt >= adc_pars_struct.ping_pong_size) {
		adc_pars_struct.pack_cnt = 0;	/* �������� ������� */
		adc_pars_struct.ping_pong_flag = !adc_pars_struct.ping_pong_flag;	/* ������ ����. ��� ������� �� � �������� */
		ssync();

		/* ���� ��� ����� ������: handler_write_flag, ������ ������ �� SD ������������� - ��������� �� flash  */
		if (!adc_pars_struct.handler_write_flag)
			adc_error.block_timeout++;	/* ������� ������ ������ ����� �� SD */

		adc_pars_struct.long_time1 = adc_pars_struct.long_time0;	/* ���������. ����� ����� �������� */
		raise(SIGIVG14);	/* ���� ��������! */
	}
}


/**
 * ���������� �� �������� �� "1" � "0" ��� ���
 */
section("L1_code")
void ADS1282_ISR(void)
{
	u32 data[ADC_CHAN];	/* 4 ������  */

	read_data_from_spi(ADC_CHAN, data);	/* �������� � ������� BIG ENDIAN!!! ������ ���� */

	do {
#if 0
		/* ��� ������� 125 - ���� ������ 2 ������. ������� � ����� ������������������! */
		if ((adc_pars_struct.sps_code == SPS125) && (adc_pars_struct.num_irq % 2 == 1)) {
			break;
		}
#endif

		/* ��� ������ �� PC */
		if (adc_pars_struct.mode == CMD_MODE) {
			cmd_mode_handler(data, ADC_CHAN);	/* �������� �� 125  */
		} else if (adc_pars_struct.mode == WORK_MODE) {	/* ����������� ����� - ������ �� SD �����  */
			work_mode_handler(data, ADC_CHAN);
		}
	} while (0);
	adc_pars_struct.num_irq++;	/* ������� ���������� */

	adc_irq_acc();		/* ���������� � �����, ����� ����� ��������� ������ ������ ���������� */

	PLL_sleep(DEV_REG_STATE);
}

/**
 * �������� 32 ���� �� ��� ������ ����� �� ���� ������� � Little ENDIAN
 */
section("L1_code")
static void read_data_from_spi(int num, u32 data[num])
{
	int i, j;
	register u8 b0, b1, b2, b3;

	/* �������� � ������� Little ENDIAN!!! ������ ���� */
	for (i = num - 1; i >= 0; i--) {
		select_chan(i);
		b0 = SPI0_write_read(0);	// �������
		b1 = SPI0_write_read(0);	// ��.�������
		b2 = SPI0_write_read(0);	// ��. �������
		b3 = SPI0_write_read(0);	// �������

		data[i] = ((u32) b0 << 24) | ((u32) b1 << 16) | ((u32) b2 << 8) | ((u32) b3);


		/* ��� �������� ��������� ��� ����� �����, ���� ���� ���� - ����� ����� ������ � ������� */
#if defined ENABLE_TEST_DATA_PACK
		if (i == 2) {
			int d;
			j = get_usec_ticks() / 25;	// ���� ������������
			d = get_sin_table(j % SIN_TABLE_SIZE) - 0x3fff;
			data[i] = d << 8;
			adc_error.test_counter++;
		}
#endif
	}
}

/**
 * ������ - ��������� 1...4 ���� � ������� ��� ����� ������� ��� ���������� ������ PING_PONG
 */
section("L1_code")
static void signal_handler(int s)
{
	u8 *ptr;
	u64 ns = adc_pars_struct.long_time1;	/* �������� � ������������ */
	adc_pars_struct.handler_write_flag = false;	/* ������� ���� ������� - ������ � ���������� */

	/*  ������ �������� ����� ��� ������� - �� ������� ���������� � ISR */
	if (0 == adc_pars_struct.num_sig % adc_pars_struct.sig_in_time) {
		log_create_hour_data_file(ns);
	}

	/* 1 ��� � ������ ��������� ��������� � ������������ ��������  */
	if (0 == adc_pars_struct.num_sig % adc_pars_struct.sig_in_min) {
		log_write_adc_header_to_file(ns);	/* ������������ ����� � ��������� */
	}


	/* C��������� ����� �� SD ����� */
	if (1 == adc_pars_struct.ping_pong_flag) {	/* ����� ������ ���� */
		ptr = ADC_DATA_ping;
	} else {		/* ����� ������ ���� */
		ptr = ADC_DATA_pong;
	}

	log_write_adc_data_to_file(ptr, adc_pars_struct.ping_pong_size * adc_pars_struct.data_size);

	adc_pars_struct.num_sig++;	/* ���������� ������ �� 4, 2 ��� 1 ������� */
	adc_pars_struct.handler_sig_flag = true;	/* 1 ��� � ������� - ������� ��������� */
	adc_pars_struct.handler_write_flag = true;	/* ������ ���� - ������� �� �����������. */
	signal(SIGIVG14, signal_handler);	/* ����� ����������� ������ �� IVG14 */
	PLL_sleep(DEV_REG_STATE);	/* vvvvv: ��������� � ������ ����������� - �����! */
}

/**
 * ���������� ������� ������
 */
#pragma section("FLASH_code")
void ADS1282_get_error_count(ADS1282_ERROR_STRUCT * err)
{
	if (err != NULL) {
		memcpy(err, &adc_error, sizeof(adc_error));
	}
}

/**
 * ������� ��� � PD - ����� ����������
 */
#pragma section("FLASH_code")
void ADS1282_standby(void)
{
	int i;

	pin_config();
	for (i = 0; i < ADC_CHAN; i++) {
		select_chan(i);	/* �������� ��� */
		cmd_write(SDATAC);
		delay_ms(5);
		cmd_write(STANDBY);
	}
}

/**
 * ������� offset cal
 */
#pragma section("FLASH_code")
bool ADS1282_ofscal(void)
{
	int i;

	/* �� ����� �������!  */
	if (adc_pars_struct.is_run)
		return false;

	pin_config();
	for (i = 0; i < ADC_CHAN; i++) {
		select_chan(i);	/* �������� ��� */
		cmd_write(OFSCAL);
	}
	return true;
}

/**
 * ������� gain cal
 */
#pragma section("FLASH_code")
bool ADS1282_gancal(void)
{
	int i;
	/* �� ����� �������!  */
	if (adc_pars_struct.is_run)
		return false;

	pin_config();
	for (i = 0; i < ADC_CHAN; i++) {
		select_chan(i);	/* �������� ��� */
		cmd_write(GANCAL);
	}
	return true;
}



/**
 * ��� �������?
 */
#pragma section("FLASH_code")
bool ADS1282_is_run(void)
{
	return adc_pars_struct.is_run;
}

/**
 * �������� ����� ����������
 */
#pragma section("FLASH_code")
int ADS1282_get_irq_count(void)
{
	return adc_pars_struct.test_irq_num;	/* ������� ���������� */
}


/**
 *  ��� ������� ���������� ������ �� ������
 */
section("L1_code")
bool ADS1282_get_pack(void *buf)
{
	if (!cb_is_empty(&cb) && buf != NULL) {
		cb_read(&cb, (ElemType *) buf);
		return true;
	} else {
		return false;	//  "������ �� ������"
	}
}

/**
 * �������� ���������� �������������� ��������� ��� n-�� ������
 */
#pragma section("FLASH_code")
bool ADS1282_get_adc_const(u8 ch, u32 * offset, u32 * gain)
{
	/* ����� ���� ������ 4 ������  */
	if (offset == NULL || gain == NULL || ch > 3 || adc_pars_struct.regs.magic != MAGIC)
		return false;

	/* �������� �� ��������� */
	*offset = adc_pars_struct.regs.chan[ch].offset;
	*gain = adc_pars_struct.regs.chan[ch].gain;
	return true;
}

/**
 * �������� ���������� ���������� ��� n-�� ������ � ���������
 */
#pragma section("FLASH_code")
bool ADS1282_set_adc_const(u8 ch, u32 offset, u32 gain)
{
	if (!adc_pars_struct.is_run && ch == 0xff) {	// �������� �� flash
		adc_pars_struct.regs.magic = MAGIC;	// ������ ���������� �����
		if (write_all_ads1282_coefs_to_eeprom(&adc_pars_struct.regs))	// ������� ������� ������ ��������
			return false;
		return true;
	}

	/* ������ ������ �������� ����������� ���, ����� ���� ������ 4 ������, �������� �� ����� ���� == 0  */
	if (adc_pars_struct.is_run || gain == 0 || ch > 3) {
		return false;
	}

	/* �������� � � ��������� */
	adc_pars_struct.regs.chan[ch].offset = offset;
	adc_pars_struct.regs.chan[ch].gain = gain;
	return true;
}


/**  
 *  �������� ����� ������
 */
#pragma section("FLASH_code")
bool ADS1282_clear_adc_buf(void)
{
	cb_clear(&cb);
	return true;
}


/**
 *  �������� � �������� ��������� 1 - �� ������
 *  ������ ������ �������� ����������� ���
 */
#pragma section("FLASH_code")
static bool write_adc_const(u8 ch, u32 offset, u32 gain)
{
	/* ����� ���� ������ 4 ������, �������� �� ����� ���� == 0  */
	if (gain == 0 || ch > 3 || adc_pars_struct.is_run)
		return false;

	select_chan(ch);	/* �������� ��� */
	reg_write(OFC0, offset & 0xFF);
	reg_write(OFC1, offset >> 8);
	reg_write(OFC2, offset >> 16);

	reg_write(FSC0, gain & 0xFF);
	reg_write(FSC1, gain >> 8);
	reg_write(FSC2, gain >> 16);
	return true;
}

/**
 *  ��������� ��������� ������ ������
 */
#pragma section("FLASH_code")
static bool read_adc_const(u8 ch, u32 * offset, u32 * gain)
{
	u8 byte0, byte1, byte2;

	/* ������ ������ �������� ����������� ���, ����� ���� ������ 4 ������, �������� �� ����� ���� == 0  */
	if (gain == 0 || ch > 3 || adc_pars_struct.is_run)
		return false;

	select_chan(ch);	/* �������� ��� */

	byte0 = reg_read(OFC0);
	byte1 = reg_read(OFC1);
	byte2 = reg_read(OFC2);
	*offset = (byte2 << 16) | (byte1 << 8) | (byte0);

	byte0 = reg_read(FSC0);
	byte1 = reg_read(FSC1);
	byte2 = reg_read(FSC2);
	*gain = (byte2 << 16) | (byte1 << 8) | (byte0);
	return true;
}


/**
 * ������ ���� ������ �������
 */
#pragma section("FLASH_code")
bool ADS1282_get_handler_flag(void)
{
	return adc_pars_struct.handler_sig_flag;
}

/**
 * �������� ���� ������ �������
 */
#pragma section("FLASH_code")
void ADS1282_reset_handler_flag(void)
{
	adc_pars_struct.handler_sig_flag = false;
}


/**
 * ����������� ����������
 */
#pragma always_inline
static inline void adc_irq_acc(void)
{
	*pPORTFIO_CLEAR = _1282_DRDY;
	ssync();
}

/* �������� ����������  */
#pragma section("FLASH_code")
void ADS1282_stop_irq(void)
{
	*pSIC_IMASK0 &= ~IRQ_PFA_PORTF;
	asm("ssync;");
}

/**
 * ������ ��� � �������������� �����������
 */
#pragma section("FLASH_code")
static void irq_register(void)
{
	/* ���������� ��������� �� �������� �� ������� � ����,
	 * ��� ����� �������� ��� 1 ��� �������� � 0 */
	*pPORTFIO_POLAR |= _1282_DRDY;
	*pPORTFIO_EDGE |= _1282_DRDY;
	*pPORTFIO_MASKA_SET |= _1282_DRDY;	/* ��������� ����������  - ����� A */

	*pSIC_IAR3 &= 0xFF0FFFFF;
	*pSIC_IAR3 |= 0x00100000;
	asm("ssync;");
}



/**
 * ������ ������������ ����� DRDY � OWF � ������������� ������������� ���
 * ������ �� ���� ��� �����!
 */
#pragma section("FLASH_code")
static void pin_config(void)
{
	*pPORTF_FER &= ~(_1282_OWFL | _1282_DRDY);	/* ��������� ������� */
	*pPORTFIO_INEN |= (_1282_OWFL | _1282_DRDY);	/* ����� ������������ � ���������� �� ���� */
	*pPORTF_FER &= ~(CH_SEL_A | CH_SEL_B);	/* ��������� ������� */
	*pPORTFIO_DIR |= CH_SEL_A | CH_SEL_B;	/* ����� ���� ������� �� ����� */
	asm("ssync;");
	SPI0_init();		/* ��� ��� ������� */
}



/*****************************************************************************************
 * ������������� ���� 4-� ��� � BlackFin �� ������� 192 ���
 * ������ �������� SYNC ������� 0.5 ��� 
 * ���� ��� ������� ���������� �� ���������� - �� ����� �������� ������
 * �� ����� ��� �� ������!
 *****************************************************************************************/
#pragma section("FLASH_code")
static inline void adc_sync(void)
{
	/* ��������� ������� ��� SYNC */
	*pPORTG_FER &= ~_1282_SYNC;
	*pPORTGIO_DIR |= _1282_SYNC;
	*pPORTGIO_CLEAR = _1282_SYNC;	/* sync ���������� � ���� */
	*pPORTGIO_SET = _1282_SYNC;
	asm("ssync;");

	/* ����� ������� 0.5 ��� - ! */
	delay_us(10);

	*pPORTGIO_CLEAR = _1282_SYNC;
	asm("ssync;");

	*pPORTFIO_CLEAR = _1282_DRDY;	/* ��� ��������� � start_irq   */
	*pILAT |= EVT_IVG8;	/* clear pending IVG8 interrupts */
	ssync();
}


 /************************************************************************
 * ������ ������� � ��� 
 * ��������: ������� 
 * �������:  ���
 ************************************************************************/
#pragma section("FLASH_code")
static void cmd_write(u8 cmd)
{
	int volatile i;

	/* �������� ������� */
	SPI0_write_read(cmd);

	/* ����� � 24 ����� Fclk ������� ~5 ����������� */
	delay_us(10);
}

/************************************************************************
 * ������ � 1 �������  ���
 * ��������:  ����� ��������, ������
 * �������:  ���
 ************************************************************************/
#pragma section("FLASH_code")
static void reg_write(u8 addr, u8 data)
{
	int volatile i, j, z;
	u8 volatile cmd[3];

	cmd[0] = 0x40 + addr;
	cmd[1] = 0;
	cmd[2] = data;


	for (i = 0; i < 3; i++) {
		SPI0_write_read(cmd[i]);

		/* ����� � ~24 ����� Fclk (5 ���) - �������! */
		delay_us(10);
	}
}

/************************************************************************
 * ��������� 1 �������  ���: 
 * ��������:  ����� ��������
 * �������:   ������
 ************************************************************************/
#pragma section("FLASH_code")
static u8 reg_read(u8 addr)
{
	int volatile i, j;
	u8 volatile cmd[2];
	u8 volatile data;

	cmd[0] = 0x20 + addr;
	cmd[1] = 0;

	for (i = 0; i < 2; i++) {
		SPI0_write_read(cmd[i]);


		/* ����� � ~24 ����� Fclk (5 ���) - �������! */
		delay_us(10);
	}
	data = SPI0_write_read(0);
	return data;
}

/**
 * ����� ��� (��������) ��� �����������
 */
#pragma section("FLASH_code")
static void adc_reset(void)
{
	pin_clr(_1282_RESET_PORT, _1282_RESET_PIN);	/* �������� RESET# ��� �� PC7 */
	delay_ms(5);		/* ��������  */
	pin_set(_1282_RESET_PORT, _1282_RESET_PIN);	/*  ��������� �����  */
}


/**
 * ����� ��� - �������������! 00 - 0, 01 - 1, 10 - 2, 11 - 3,  ���� ��� � ����
 * ���� ��� ������������ ������ - ����� ������ ������ ���� �����! 
 */
section("L1_code")
static void select_chan(int chan)
{
	switch (chan) {

		/* ������ ����� ��� */
	case 0:
		*pPORTFIO_CLEAR = CH_SEL_B | CH_SEL_A;
		ssync();
		break;

		/* ������ ����� ��� */
	case 1:
		*pPORTFIO_SET = CH_SEL_A;
		*pPORTFIO_CLEAR = CH_SEL_B;
		ssync();
		break;

		/* ������ ����� ��� */
	case 2:
		*pPORTFIO_CLEAR = CH_SEL_A;
		*pPORTFIO_SET = CH_SEL_B;
		ssync();
		break;

		/* ��������� ����� ��� */
	case 3:
	default:
		*pPORTFIO_SET = CH_SEL_B | CH_SEL_A;
		ssync();
		break;
	}
}
