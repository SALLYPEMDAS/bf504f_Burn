/*******************************************************************************************************************
 * ���� ������� ������� BQ32000
 *  � FAST ������:
 * SCL clock low time - 1.3 ��� �������
 * SCL clock hi  time - 0.6 ��� �������
 *
 *  � ����������� ������:
 * SCL clock low time - 4.7 ��� �������
 * SCL clock hi  time - 4.0 ��� �������
 *
 *******************************************************************************************************************/

#include <time.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include "timer0.h"
#include "utils.h"
#include "bq32k.h"
#include "led.h"
#include "log.h"
#include "twi.h"

#define 	BQ32_TWI_DIV_HI		66
#define 	BQ32_TWI_DIV_LO         67
#define 	BQ32_NUM_MOD		1


/**
 * ��������� ���������� TWI
 */
section("FLASH_data")
static const twi_clock_div bq32_val = {
    .lo_div_clk = BQ32_TWI_DIV_LO,
    .hi_div_clk = BQ32_TWI_DIV_HI,
    .rsvd = BQ32_NUM_MOD
};


/* ����� ����� �� ������ � �� ������ */
#define BQ32K_ADDR	0x68


/* �������� bq32000.  ���������� */
#define BQ32K_SECONDS           0x00	/* Seconds register address */
#define BQ32K_SECONDS_MASK      0x7F	/* Mask over seconds value */
#define BQ32K_STOP              0x80	/* Oscillator Stop flag */

#define BQ32K_MINUTES           0x01	/* Minutes register address */
#define BQ32K_MINUTES_MASK      0x7F	/* Mask over minutes value */
#define BQ32K_OF                0x80	/* Oscillator Failure flag */


#define BQ32K_HOURS		0x02
#define BQ32K_HOURS_MASK        0x3F	/* Mask over hours value */
#define BQ32K_CENT              0x40	/* Century flag */
#define BQ32K_CENT_EN           0x80	/* Century flag enable bit */


#define BQ32K_DAY		0x03
#define BQ32K_DATE		0x04
#define BQ32K_MONTH		0x05
#define BQ32K_YEARS		0x06
#define BQ32K_CAL_CFG1		0x07
#define BQ32K_TCH2		0x08
#define BQ32K_CFG2		0x09

/* ����������� */
#define BQ32K_SFKEY1		0x20
#define BQ32K_SFKEY2		0x21
#define BQ32K_SFR		0x22

/* �������� ��������� ����� */
typedef struct {
    u8 seconds;
    u8 minutes;
    u8 cent_hours;
    u8 day;
    u8 date;
    u8 month;
    u8 years;
} bq32k_regs;


/* ������� ������� ��� �������� � ���������� ��� � �������� */
#pragma section("FLASH_code")
static unsigned bcd2bin(u8 val)
{
    return (val & 0x0f) + (val >> 4) * 10;
}

#pragma section("FLASH_code")
static u8 bin2bcd(unsigned val)
{
    return ((val / 10) << 4) + val % 10;
}


/* ���������� ���� � RTC */
#pragma section("FLASH_code")
void set_rtc_sec_ticks(long sec)
{
    struct tm *tm_ptr;
    bq32k_regs regs;

    tm_ptr = gmtime(&sec);	/* ��������� ����� � tm_time */

    if (tm_ptr != NULL) {
	regs.seconds = bin2bcd(tm_ptr->tm_sec) & BQ32K_SECONDS_MASK;
	regs.minutes = bin2bcd(tm_ptr->tm_min) & BQ32K_MINUTES_MASK;

	regs.cent_hours = bin2bcd(tm_ptr->tm_hour) | BQ32K_CENT_EN;

	regs.day = bin2bcd(tm_ptr->tm_wday + 1);
	regs.date = bin2bcd(tm_ptr->tm_mday);
	regs.month = bin2bcd(tm_ptr->tm_mon + 1);

	if (tm_ptr->tm_year >= 100) {
	    regs.cent_hours |= BQ32K_CENT;
	    regs.years = bin2bcd(tm_ptr->tm_year - 100);
	} else
	    regs.years = bin2bcd(tm_ptr->tm_year);

	/* ���������� ����� I2C � ���� */
	TWI_write_pack(BQ32K_ADDR, 0, (u8 *) & regs, sizeof(bq32k_regs), &bq32_val);
    }
}

/* �������� ���� �� RTC � �������� */
#pragma section("FLASH_code")
long get_rtc_sec_ticks(void)
{
    bq32k_regs regs;
    struct tm tm;
    long t0 = -1;

    /* ������ ���� �� RTC �� I2C */
    if (TWI_read_pack(BQ32K_ADDR, 0, (u8 *) & regs, sizeof(bq32k_regs), &bq32_val) == true) {

	tm.tm_isdst = 0;	/* ����������� - ����� ����� ������ ��� */
	tm.tm_sec = bcd2bin(regs.seconds & BQ32K_SECONDS_MASK);
	tm.tm_min = bcd2bin(regs.minutes & BQ32K_MINUTES_MASK);
	tm.tm_hour = bcd2bin(regs.cent_hours & BQ32K_HOURS_MASK);
	tm.tm_mday = bcd2bin(regs.date);
	tm.tm_wday = bcd2bin(regs.day) - 1;
	tm.tm_mon = bcd2bin(regs.month) - 1;
	tm.tm_year = bcd2bin(regs.years) + ((regs.cent_hours & BQ32K_CENT) ? 100 : 0);
	t0 = mktime(&tm);

	/* �� ���� ��������� ���� - ����� �� ������ */
	if(t0 < 10)
            t0 = -1;
    }
    return t0;
}
