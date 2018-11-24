/************************************************************************************** 
 * � ���� ����� ����� ����������� � ��������� SPORT ��� ����� � ������� �������
 * ���� ������� ��������� ������ � ����� ������ � ��� � ����� � ������� ������
 * ������ ���� ������� ������ ������������� ������������� ������ ��������� �����!!!
***************************************************************************************/
#include "xpander.h"
#include "bq32k.h"
#include "utils.h"
#include "sport0.h"
#include "ports.h"
#include "bmp085.h"
#include "lsm303.h"
#include "rele.h"
#include "log.h"
#include "led.h"
#include "pll.h"

/**
 * ���������� ��� ��� ���������
 */
static struct {
    long time;
    u8 num;			// ����� ������������
    bool err;
} adc_error_init;


static u8 XPANDER_read_byte(u8);
static void XPANDER_write_byte(u8, u8);
static void XPANDER_set_bit(u8, u8);
static void XPANDER_clear_bit(u8, u8);
static void XPANDER_toggle_bit(u8, u8);
static u16 XPANDER_adc_get_data(u8);
static void XPANDER_adc_init(void);

#define 	XPANDER_ADC_TIMEOUT		10


/**
 * ��������������� ��� �������, ����������� �� TWI � � ������
 */
#pragma section("FLASH_code")
void adc_init(void *par)
{
    DEV_STATUS_STRUCT *status;

    if (par) {
	status = (DEV_STATUS_STRUCT *) par;

	// �������������� ������
	XPANDER_adc_init();

	// �������������� ������ �����������
	if (bmp085_init() == false) {
	    status->st_test0 |= 0x02;	// ������ ������
	}
	// ���������� � ��������
	if (lsm303_init_acc() == false) {
	    status->st_test0 |= 0x04;	// ������ ������
	}

	if (lsm303_init_comp() == false) {
	    status->st_test0 |= 0x08;	// ������ ������
	}
    }
}




/**
 * "�������" ������� - �������� ���������� � ��� � ���, ����� � ������
 *  ��������� ������ �������
 */
#pragma section("FLASH_code")
bool adc_get(void *par)
{
    lsm303_data acc, comp;
    DEV_STATUS_STRUCT *status;
    int t, p;
    u16 u, i;

    if (par == NULL)
	return false;

    status = (DEV_STATUS_STRUCT *) par;

    // ��� ����� ���� 
#if defined  GNS110_R2B_BOARD || defined GNS110_R2C_BOARD
    /* 4 ������ ���������� */
    u = XPANDER_adc_get_data(0);
    t = u * ATMEGA_VOLT_COEF;
    status->am_power_volt = t;

    u = XPANDER_adc_get_data(1);
    t = u * ATMEGA_VOLT_COEF;
    status->burn_ext_volt = t;

    u = XPANDER_adc_get_data(2);
    t = u * ATMEGA_VOLT_COEF;
    status->burn_volt = t;

    u = XPANDER_adc_get_data(3);
    t = u * ATMEGA_VOLT_COEF;
    status->regpwr_volt = t;

#if 10				// ��� �����
    if (t == 0) {
	adc_error_init.err = true;
	if (get_sec_ticks() != adc_error_init.time) {
	    adc_init(status);
	}
    }
#endif


    /* 3 ������ ���� */
    i = XPANDER_adc_get_data(5);
    t = i * ATMEGA_AMPER_SUPPLY_COEF;
    status->ireg_sense = t - 8;

    i = XPANDER_adc_get_data(4);
    t = i * ATMEGA_AMPER_AM3_COEF;
    t = (t - 16) * 100 / 354;
    status->iam_sense = t;

    i = XPANDER_adc_get_data(6);
    t = i * ATMEGA_AMPER_BURN_COEF;
    status->iburn_sense = (t - 80);

#else				/* ����� A */
    u = XPANDER_adc_get_data(0);
    t = u * ATMEGA_VOLT_COEF;
    status->regpwr_volt = t;	// ��������� ������������� :(

    u = XPANDER_adc_get_data(1);
    t = u * ATMEGA_VOLT_COEF;
    status->burn_volt = t;

    i = XPANDER_adc_get_data(2);
    t = i * ATMEGA_AMPER_SUPPLY_COEF;
    status->ireg_sense = t - 8;

    i = XPANDER_adc_get_data(3);
    t = i * ATMEGA_AMPER_BURN_COEF;
    status->iburn_sense = t - 80;

    status->am_power_volt = 0;
    status->burn_ext_volt = 0;
    status->iam_sense = 0;
#endif

	    /* ���� � ��� ������ ��� ����������� - ����� ����� RTC � ������ */
#if !defined		GNS110_PROJECT
	    /* ���� ��� ������������ */
	    if (!(status->st_main & 0x08) && (t = get_rtc_sec_ticks()) != -1) {
		status->gns_rtc = t;	// ���� RTC
		status->st_test0 &= ~0x01;	// ����� �������������
	    } else {
		status->st_test0 |= 0x01;	// ������������� ���������
	    }
#endif

    // ���� ��� ������ ������� �������� �� I2C
    if (bmp085_data_get(&t, &p) == true) {
	status->temper1 = t;
	status->press = p;
	status->st_test0 &= ~0x02;	// ������������� �����
    } else {
	status->temper1 = 0;
	status->press = 0;
	status->st_test0 |= 0x02;	// �������������
    }

    /* ������ ������������� */
    if (lsm303_get_acc_data(&acc) == true) {
	status->st_test0 &= ~0x04;	// ������������� �����
    } else {
	status->st_test0 |= 0x04;	// �������������
    }

    /* ������ ������� + ����������� */
    if (lsm303_get_comp_data(&comp) == true) {
	status->temper0 = comp.t;	// �����������
	status->st_test0 &= ~0x08;	// ������������� �����
    } else {
	status->temper0 = 0;	// �����������
	status->st_test0 |= 0x08;	// �������������
    }

    /* ��������� ���� */
    if (!(status->st_test0 & 0x04) && !(status->st_test0 & 0x08) && calc_angles(&acc, &comp)) {
	status->pitch = acc.x;
	status->roll = acc.y;
	status->head = acc.z;
    }

    return true;		/*vvvv �������� ������ ��� ����. ������!  */
}

/**
 * ������ �������������� ��� ������. �������� ���������� �� ������ ��� ������ 
*/
#pragma section("FLASH_code")
static u16 XPANDER_adc_get_data(u8 ch)
{
    u8 byte;
    u16 result = 0;
    int timeout = XPANDER_ADC_TIMEOUT;
    int num;


    /* �������� ����� ������ */
    if (ch < 4) {
	num = 0;

#if defined  GNS110_R2A_BOARD
	num = ch;
#endif

	switch (ch) {
	case 0:
	    XPANDER_clear_bit(MES_MUX_SELA_PORT, 1 << MES_MUX_SELA_PIN);
	    XPANDER_clear_bit(MES_MUX_SELB_PORT, 1 << MES_MUX_SELB_PIN);
	    break;

	case 1:
	    XPANDER_set_bit(MES_MUX_SELA_PORT, 1 << MES_MUX_SELA_PIN);
	    XPANDER_clear_bit(MES_MUX_SELB_PORT, 1 << MES_MUX_SELB_PIN);
	    break;

	case 2:
	    XPANDER_clear_bit(MES_MUX_SELA_PORT, 1 << MES_MUX_SELA_PIN);
	    XPANDER_set_bit(MES_MUX_SELB_PORT, 1 << MES_MUX_SELB_PIN);
	    break;

	default:
	    XPANDER_set_bit(MES_MUX_SELA_PORT, 1 << MES_MUX_SELA_PIN);
	    XPANDER_set_bit(MES_MUX_SELB_PORT, 1 << MES_MUX_SELB_PIN);
	    break;
	}
    } else {
	num = ch - 3;
    }

    /* ��������� 3 ������� ���� � �������������� */
    byte = 0x40;
    byte |= (u8) num;

    /* �������� ����� ��� */
    XPANDER_write_byte(ADMUX, byte);
    delay_ms(10);

    /* ������ ��������� */
    XPANDER_set_bit(ADCSRA, (1 << PIN6));

    /* ���� ���������� �������������� �� ����� ��������� ������ */
    while (--timeout) {

	byte = XPANDER_read_byte(ADCSRA);
	if ((byte & (1 << PIN4))) {	/* ������ ������ - ��������� ���������� */
	    XPANDER_set_bit(ADCSRA, (1 << PIN4));

	    result = XPANDER_read_byte(ADCL);
	    result |= XPANDER_read_byte(ADCH) << 8;
	    break;
	}
    }

    return result;
}

/***************************************************************************************** 
 * ���������������� ��� �� ������ 
 ****************************************************************************************/
#pragma section("FLASH_code")
static void XPANDER_adc_init(void)
{
    /* ����� */
    XPANDER_clear_bit(ADC_PORT_DIR, ADC_PORT_PIN);

    /* ������ ����� �������� ������� ���������������� ADC ����� */
    XPANDER_clear_bit(PRR, (1 << PRADC));

    /* ������ ������� ADMUX - ������� ��������: 3.3 V  - AVCC with external capacitor at AREF pin 6 ���
     * ����� ������ �����, ��� ������������ ���������� ����� */
    XPANDER_write_byte(ADMUX, (1 << PIN6));

    /* ������ ������� ADCSRA - ���� �� ���������, ��������� �� 128 (������� 62 ���)  */
    XPANDER_write_byte(ADCSRA, (1 << PIN7) | (1 << PIN2) | (1 << PIN1) | (1 << PIN0));

    /* ������ ������� ADCSRB = 0. Free Running Mode */
    XPANDER_write_byte(ADCSRB, 0);

    /* �������� �������������� �������� ����� ���������, ������ ��������� */
    XPANDER_write_byte(DIDR0, 0xFF);

    /* 1 ��� ��������� */
    adc_error_init.num++;
    adc_error_init.err = false;
    adc_error_init.time = get_sec_ticks();
}


/**************************************************************************************** 
 * ������ ����� �� ����� - ��� ������ ���� �� 3 ����� 
 * � ������ ������ ������� ������ "0"
 * ��������: �����
 * �������:  ������
 ****************************************************************************************/
#pragma section("FLASH_code")
static u8 XPANDER_read_byte(u8 addr)
{
    u8 byte;

    SPORT0_write_read(0);
    delay_us(10);
    SPORT0_write_read(addr);
    delay_us(10);
    byte = SPORT0_write_read(0);
    delay_us(10);

    return byte;
}


/*************************************************************************************** 
 * ������� ��������� ������� �� SPORT � ���������, ������ ���� �� 3 �����
 * �������� ����� ��������� - 10 ���. ��������� ��������� ���� ������� �� 0.125 ���
 * � ������ ������ ������� ������ "0x80"
 * ��������: �����, ������
 * �������:  ���
  ***************************************************************************************/
#pragma section("FLASH_code")
static void XPANDER_write_byte(u8 addr, u8 data)
{
    SPORT0_write_read(0x80);
    delay_us(10);
    SPORT0_write_read(addr);
    delay_us(10);
    SPORT0_write_read(data);
    delay_us(10);
}

/*************************************************************************************** 
 * ��������� 1-�� ���� �� ������
 * �������� ����� ��������� - 10 ���. ��������� ��������� ���� ������� �� 0.125 ���
 * ������ ������� ������ "0x20"
 * ��������: �����, ������
 * �������:  ���
  ***************************************************************************************/
#pragma section("FLASH_code")
static void XPANDER_set_bit(u8 addr, u8 data)
{
    SPORT0_write_read(0x20);
    delay_us(10);
    SPORT0_write_read(addr);
    delay_us(10);
    SPORT0_write_read(data);
    delay_us(10);
}

/*************************************************************************************** 
 * ����� 1-�� ���� �� ������
 * �������� ����� ��������� - 10 ���. ��������� ��������� ���� ������� �� 0.125 ���
 * ������ ������� ������ "0x10"
 * ��������: �����, ������
 * �������:  ���
  ***************************************************************************************/
#pragma section("FLASH_code")
static void XPANDER_clear_bit(u8 addr, u8 data)
{
    SPORT0_write_read(0x10);
    delay_us(10);
    SPORT0_write_read(addr);
    delay_us(10);
    SPORT0_write_read(data);
    delay_us(10);
}

/*************************************************************************************** 
 * ������������ 1-�� ���� �� ������
 * �������� ����� ��������� - 10 ���. ��������� ��������� ���� ������� �� 0.125 ���
 * ������ ������� ������ "0x10"
 * ��������: �����, ������
 * �������:  ���
  ***************************************************************************************/
#pragma section("FLASH_code")
static void XPANDER_toggle_bit(u8 addr, u8 data)
{
    SPORT0_write_read(0x40);
    delay_us(10);
    SPORT0_write_read(addr);
    delay_us(10);
    SPORT0_write_read(data);
    delay_us(10);
}

#pragma section("FLASH_code")
void pin_set(u8 port, u8 pin)
{
    u8 dir = port - 1;
    XPANDER_set_bit(port, 1 << pin);
    XPANDER_set_bit(dir, 1 << pin);
}

#pragma section("FLASH_code")
void pin_clr(u8 port, u8 pin)
{
    u8 dir = port - 1;
    XPANDER_clear_bit(port, 1 << pin);
    XPANDER_set_bit(dir, 1 << pin);
}

#pragma section("FLASH_code")
void pin_hiz(u8 port, u8 pin)
{
    u8 dir = port - 1;
    XPANDER_clear_bit(port, 1 << pin);
    XPANDER_clear_bit(dir, 1 << pin);
}

#pragma section("FLASH_code")
u8 pin_get(u8 port)
{
    return XPANDER_read_byte(port);
}

/**
 * ��������� ������� ���������������� ADC �������� 
 */
#pragma section("FLASH_code")
void adc_stop(void)
{
    XPANDER_set_bit(PRR, (1 << PRADC));
}
