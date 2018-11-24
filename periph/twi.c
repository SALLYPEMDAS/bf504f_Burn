/* �� TWI (I2C) ��������:
 * 1) RTC
 * 2) ������ ����������� � ��������
 * 3) ������������ � ������
 * + ����� ��������� ������ ����������!
 * NB - � ������� ������ ������ ����������� ������ �������� �������! TWI ������� "�� �������!"
 */

#include "utils.h"
#include "pll.h"
#include "twi.h"

////vvvvv:
/* #define PRESCALER_VALUE 	5  */
#define PRESCALER_VALUE 	(TIMER_FREQ_MHZ / 10 + 1)	/* PRESCALER = SCLK / 10MHz  */

#define RESET_TWI 		0	/* RESET_TWI value for controller */
#define CLKDIV_HI 		16	/* SCL high period */
#define CLKDIV_LO 		17	/* SCL low period */

/** 
 * ������������� TWI, �� �� ��������� � GPIO :( - ������ ������ �����.
 * ��������� �������� �������� ������ � ������ � ������-��� ������ ���������
 * ��� ����� ����� ������ ���� 
 */
#pragma section("FLASH_code")
static void _twi_reset(void)
{
    *pTWI_CONTROL = RESET_TWI;	/* reset TWI controller */
    *pTWI_MASTER_STAT = BUFWRERR | BUFRDERR | LOSTARB | ANAK | DNAK;	/* clear errors before enabling TWI */
    *pTWI_INT_STAT = SINIT | SCOMP | SERR | SOVF | MCOMP | MERR | XMTSERV | RCVSERV;	/* clear interrupts before enabling TWI */
    *pTWI_FIFO_CTL = XMTFLUSH | RCVFLUSH;	/* flush rx and tx fifos */
    ssync();
}

/**
 * ���� TWI - ���������� ��� � �������, ��� ��������� ����� ������ ���� ��������
 */
#pragma section("FLASH_code")
void TWI_stop(void)
{
    *pTWI_CONTROL = RESET_TWI;	/* reset TWI controller */
     ssync();
}

/** 
 * ������ � ������� �� TWI � ������ ������� 
 * ���������� ����� ��� ���
 */
#pragma section("FLASH_code")
bool TWI_write_pack(u16 addr, u8 reg, u8 * pointer, u16 count, const void* par)
{
    int i;
    bool res = true;
    u64 t0;
    u16 lo = CLKDIV_LO, hi = CLKDIV_HI;
    twi_clock_div* div;

    if(par != 0) {
       div = (twi_clock_div*)par;
       lo = div->lo_div_clk;
       hi = div->hi_div_clk;
    }

    _twi_reset();			/* ���������� ��������� TWI */

    *pTWI_FIFO_CTL = 0;		/* clear the bit manually */
    *pTWI_CONTROL = TWI_ENA | PRESCALER_VALUE;	/* ������� TWI_CONTROL, ��������� = 48 MHz / 10 MHz + ��������� TWI */

    *pTWI_CLKDIV = (hi << 8) | lo;	/* �������� ��� CLK = 100 ��� ~ 100 */
    *pTWI_MASTER_ADDR = addr;	/* �������� �������, ��� �� ����� �������� ������� reg */
    *pTWI_MASTER_CTL = (count + 1 << 6) | MEN;	/* ����� �������� ����� ��������, 1 ���� ������� + count �������� */
    ssync();


    /* wait to load the next sample into the TX FIFO */
    t0 = get_msec_ticks();
    while (*pTWI_FIFO_STAT == XMTSTAT) {
	ssync();
	if (get_msec_ticks() - t0 > 50) {
	    res = false;
	    goto met;
	}
    }

    *pTWI_XMT_DATA8 = reg;	/* ����� �������� */
    ssync();


    /* # of transfers before stop condition */
    for (i = 0; i < count; i++) {

	t0 = get_msec_ticks();
	while (*pTWI_FIFO_STAT == XMTSTAT) {	/* wait to load the next sample into the TX FIFO */
	    ssync();
	    if (get_msec_ticks() - t0 > 50) {
		res = false;
		goto met;
	    }
	}
	*pTWI_XMT_DATA8 = *pointer++;	/* load the next sample into the TX FIFO */
	ssync();
    }

    t0 = get_msec_ticks();
    while (*pTWI_MASTER_STAT & MPROG) {
	ssync();
	if (get_msec_ticks() - t0 > 50) {
	    res = false;
	    goto met;
	}
    }
  met:
    asm("nop;");
    return res;
}


/* ������ �� TWI � ���. ������� */
#pragma section("FLASH_code")
bool TWI_read_pack(u16 addr, u8 reg, u8 * pointer, u16 count, const void* par)
{
    int i;
    bool res = false;
    u64 t0;
    u16 lo = CLKDIV_LO, hi = CLKDIV_HI;
    twi_clock_div* div;

    if(par != 0) {
       div = (twi_clock_div*)par;
       lo = div->lo_div_clk;
       hi = div->hi_div_clk;
    }


    _twi_reset();
    *pTWI_FIFO_CTL = 0;
    *pTWI_CONTROL = TWI_ENA | PRESCALER_VALUE;	/* ������� TWI_CONTROL, ��������� = 48 MHz / 10 MHz + ��������� TWI */

    *pTWI_CLKDIV = (hi << 8) | lo;	/* �������� ��� CLK = 100 ��� ~ 100 */
    *pTWI_MASTER_ADDR = addr;	/* ����� (7-��� + ��� ������/������) */
    *pTWI_XMT_DATA8 = reg;	/* ��������� ����� ��������  */
    *pTWI_MASTER_CTL = (1 << 6) | MEN;	/* ����� �������� */
    ssync();

    /* wait to load the next sample into the TX FIFO */
    t0 = get_msec_ticks();
    while (*pTWI_MASTER_STAT & MPROG) {
	ssync();

	/* ����� �� ����������� - ��� ����������� */
	if (get_msec_ticks() - t0 > 10 || *pTWI_MASTER_STAT & ANAK) {
	    goto met;
	}
    }

/*     _twi_reset();  */
    *pTWI_FIFO_CTL = 0;
    *pTWI_CONTROL = TWI_ENA | PRESCALER_VALUE;	/* ������� TWI_CONTROL, ��������� = 48 MHz / 10 MHz + ��������� TWI */

    *pTWI_CLKDIV = (hi << 8) | lo;	/* �������� ��� CLK = 100 ��� ~ 100 */
    *pTWI_MASTER_ADDR = addr;	/* ����� (7-��� + ��� ������/������) */
    *pTWI_MASTER_CTL = (count << 6) | MEN | MDIR;	/* ����� ������ */
    ssync();


    /* for each item. ������ ������� �� 5 �� */
    for (i = 0; i < count; i++) {
	t0 = get_msec_ticks();

	/* wait for data to be in FIFO */
	while (*pTWI_FIFO_STAT == RCV_EMPTY) {
	    ssync();
	    if (get_msec_ticks() - t0 > 50) {
		goto met;
	    }
	}

	*pointer++ = *pTWI_RCV_DATA8;	/* read the data */
	ssync();
    }

    res = true;			/* ��� ���������  */
  met:
    /* service TWI for next transmission */
    *pTWI_INT_STAT = RCVSERV | MCOMP;
    ssync();
    return res;
}


/**
 * ������ 2 ����� �� �������
 * �������� �������, ��� �� ����� ��������� �������� reg
 */
#pragma section("FLASH_code")
u8 TWI_read_byte(u16 addr, u8 reg, const void* par)
{
    int i;
    u8 byte;
    u64 t0;
    u16 lo = CLKDIV_LO, hi = CLKDIV_HI;
    twi_clock_div* div;

    if(par != 0) {
       div = (twi_clock_div*)par;
       lo = div->lo_div_clk;
       hi = div->hi_div_clk;
    }

     _twi_reset();
    *pTWI_FIFO_CTL = 0;
    *pTWI_CONTROL = TWI_ENA | PRESCALER_VALUE;	/* ������� TWI_CONTROL, ��������� = 48 MHz / 10 MHz + ��������� TWI */

    *pTWI_CLKDIV = (hi << 8) | lo;	/* �������� ��� CLK = 100 ��� ~ 25 */
    *pTWI_MASTER_ADDR = addr;	/* ����� (7-��� + ��� ������/������) */
    *pTWI_MASTER_CTL = (1 << 6) | MEN;	/* ����� �������� */
     ssync();

    /* wait to load the next sample into the TX FIFO */
    t0 = get_msec_ticks();
    while (*pTWI_FIFO_STAT == XMTSTAT) {
	ssync();
	if (get_msec_ticks() - t0 > 50) {
	    goto met;
	}
    }

    *pTWI_XMT_DATA8 = reg;

    /* wait to load the next sample into the TX FIFO */
    t0 = get_msec_ticks();
    while (*pTWI_MASTER_STAT & MPROG) {
	ssync();
	if (get_msec_ticks() - t0 > 50) {
	    goto met;
	}
    }


    _twi_reset();
    *pTWI_FIFO_CTL = 0;
    *pTWI_CONTROL = TWI_ENA | PRESCALER_VALUE;	/* ������� TWI_CONTROL, ��������� = 48 MHz / 10 MHz + ��������� TWI */

    *pTWI_CLKDIV = (hi << 8) | lo;	/* �������� ��� CLK = 100 ��� ~ 25 */
    *pTWI_MASTER_ADDR = addr;	/* ����� (7-��� + ��� ������/������) */
    *pTWI_MASTER_CTL = (1 << 6) | MEN | MDIR;	/* start transmission */
    ssync();


    /* wait for data to be in FIFO */
    t0 = get_msec_ticks();
    while (*pTWI_FIFO_STAT == RCV_EMPTY) {
	ssync();
	if (get_msec_ticks() - t0 > 50) {
	    goto met;
	}
    }

    byte = *pTWI_RCV_DATA8;	/* read the data */
    ssync();

  met:
    /* service TWI for next transmission */
    *pTWI_INT_STAT = RCVSERV | MCOMP;
    asm("nop;");
    asm("nop;");
    asm("nop;");
    return byte;
}


/* ������ ����� */
#pragma section("FLASH_code")
void TWI_write_byte(u16 addr, u16 reg, u8 data, const void* par)
{
    int i;
    u64 t0;
    u16 lo = CLKDIV_LO, hi = CLKDIV_HI;
    twi_clock_div* div;

    if(par != 0) {
       div = (twi_clock_div*)par;
       lo = div->lo_div_clk;
       hi = div->hi_div_clk;
    }

    _twi_reset();			/* ���������� ��������� TWI */
    *pTWI_FIFO_CTL = 0;		/* clear the bit manually */
    *pTWI_CONTROL = TWI_ENA | PRESCALER_VALUE;	/* ������� TWI_CONTROL, ��������� = 48 MHz / 10 MHz + ��������� TWI */

    *pTWI_CLKDIV = (hi << 8) | lo;	/* �������� ��� CLK = 100 ��� ~ 25 */
    *pTWI_MASTER_ADDR = addr;	/* ����� (7-��� + ��� ������/������) */
    *pTWI_MASTER_CTL = (2 << 6) | MEN;	/* ����� �������� (���� �� ����� ��������), 1 ���� ������� + count �������� */
    ssync();



    /* wait to load the next sample into the TX FIFO */
    t0 = get_msec_ticks();
    while (*pTWI_FIFO_STAT == XMTSTAT) {
	ssync();
	if (get_msec_ticks() - t0 > 50)
	    goto met;
    }

    *pTWI_XMT_DATA8 = reg;	/* ����� �������� */
    ssync();

    /* # of transfers before stop condition */
    t0 = get_msec_ticks();
    while (*pTWI_FIFO_STAT == XMTSTAT) {
	ssync();
	if (get_msec_ticks() - t0 > 50)
	    goto met;
    }

    *pTWI_XMT_DATA8 = data;	/* load the next sample into the TX FIFO */
    ssync();

    t0 = get_msec_ticks();
    while (*pTWI_FIFO_STAT == XMTSTAT) {	/* wait to load the next sample into the TX FIFO */
	ssync();
	if (get_msec_ticks() - t0 > 50)
	    goto met;
    }
  met:
    asm("nop;");
}
