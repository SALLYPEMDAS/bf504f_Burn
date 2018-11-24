/************************************************************************************** 
 * ��� ���������� BF504F � ���������� ������� 
 * CLKIN    = 19.2 ���
 * ��������� ������� � ����� ���:
 * CCLK     =  288, 230.4, 192, ->96 ���
 * SCLK     =  72, 64, 48, 32, ->24  ���
 * ����� ����� ����� �������� � ������� �������, �� ������� SCLK ������ ���� �����!
 * ����� ��������� ������� ��� ���������� ������������������ � ����
 * ������� ������� ������ �� ���������
 * ������� ����� ��������!
 * ��� ������� ���������� 19.2 ��� ����� ������� ���� � 60 ��� � ������� ��������� � 24 ���
 * CCLK = 192MHZ SCLK = 48MHZ
 **************************************************************************************/
#ifndef	__PLL_H__
#define __PLL_H__

#include "globdefs.h"
#include "config.h"




/* ������� ������ ������� - ����������� �����. �� ������� � eeprom  */
#define		CAUSE_POWER_OFF		0x12345678	/* ���������� ������� */
#define		CAUSE_EXT_RESET		0xabcdef90	/* ������� ����� */
#define		CAUSE_BROWN_OUT		0xaa55aa55	/* �������� ������� */
#define		CAUSE_WDT_RESET		0x07070707	/* WDT reset (�� ����� �����������) */
#define		CAUSE_NO_LINK		0xE7E7E7E7	/* ��� ����� - �������������� ������� */
#define		CAUSE_UNKNOWN_RESET	0xFFFFFFFF	/* ����������� �������-��������� ������� */


/* ��� ���������� 19.2 ���  */
#if QUARTZ_CLK_FREQ==(19200000)
/* ������� ��������� ����� = 24 ���
 * 19.2 ����� �� 2 = 9.6 ��� - �� ���� PLL
 * MSEL[5:0] = 25 - �������� VCO = 240 ��� �� 9.6 
 * CSEL[1:0] = 4  - �������� CCLK = VSO / 4 = 60���, 
 * SSEL[3:0] = 10 - �������� SCLK = VSO / 10 = 24��� */
#define 	SCLK_VALUE 		24000000
#define 	PLLCTL_VALUE        	((25 << 9) | 1)
#define 	PLLDIV_VALUE        	0x002A

/*
#elif QUARTZ_CLK_FREQ==(8192000)
// ������� ��������� ����� = 237568 ��� -- ������!!!
// MSEL[5:0] = 29 - �������� VCO = 237.568 ��� �� 8.192 
// CSEL[1:0] = b10  - �������� CCLK = VSO / 4 = 59.392���, 
// SSEL[3:0] =   - �������� SCLK = VSO / 10 = 24.576��� 
#define 	SCLK_VALUE 	     23756800UL	
#define 	PLLCTL_VALUE         (29 << 9)
#define 	PLLDIV_VALUE         0x002A
*/
#elif QUARTZ_CLK_FREQ==(8192000)
/* ������� ��������� ����� = 24.576 ���
 * MSEL[5:0] = 30 - �������� VCO = 245.760 ��� �� 8.192 
 * CSEL[1:0] = 0  - �������� CCLK = VSO / 4 = 61.440 ���, 
 * SSEL[3:0] = 10  - �������� SCLK = VSO / 10 = 24.576 ��� */
#define 	SCLK_VALUE 	     24576000UL	
#define 	PLLCTL_VALUE         (30 << 9)
#define 	PLLDIV_VALUE         0x002A



#elif QUARTZ_CLK_FREQ==(4096000)
/* ������� ��������� ����� = 49.152 ���
 * MSEL[5:0] = 48 - �������� VCO = 196.608��� �� 4.096 
 * CSEL[1:0] = b10  - �������� CCLK = VSO / 4 = 49.152��� 
 * SSEL[3:0] = 8  - �������� SCLK = VSO / 8 = 24.576���  */
#define 	SCLK_VALUE 	     49152000UL
#define 	PLLCTL_VALUE        (48 << 9)
#define 	PLLDIV_VALUE        0x0008

#else
#error "SCLK Value is not defined or incorrect!"
#endif


/* ��� �������� ��� Power Managements */
#define 	PLLSTAT_VALUE       0x0000			/* NB: ������ ������!!!  */
#define 	PLLLOCKCNT_VALUE    0x0200			/* ����� 512 ������ ������� */
#define 	PLLVRCTL_VALUE      ((1 << 9)|(1 << 11))	/* ����������� �� ����������� �� ����� PF8 � PF9 */

/* ������� ������� ���������� � PLL  */
#define 	TIMER_PERIOD 		(SCLK_VALUE)
#define		TIMER_HALF_PERIOD	(SCLK_VALUE / 2)
#define		TIMER_TUNE_SHIFT	(SCLK_VALUE / 1000)
#define 	TIMER_FREQ_MHZ		(SCLK_VALUE / 1000000)
#define		TIMER_NS_DIVIDER	(1000000000UL)
#define		TIMER_US_DIVIDER	(1000000)
#define		TIMER_MS_DIVIDER	(1000)


/* ��� �������� ������� SCLK */
#define		SCLK_PERIOD_NS	(TIMER_NS_DIVIDER / TIMER_PERIOD)


void PLL_init(void);
void PLL_sleep(DEV_STATE_ENUM);
void PLL_reset(void);
void PLL_fullon(void);
void PLL_hibernate(DEV_STATE_ENUM);

#endif				/* pll.h */
