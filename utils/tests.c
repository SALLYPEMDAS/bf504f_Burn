#include "tests.h"
#include "main.h"
#include "eeprom.h"
#include "ports.h"
#include "bq32k.h"
#include "ads1282.h"
#include "xpander.h"
#include "bmp085.h"
#include "lsm303.h"
#include "timer1.h"
#include "timer2.h"
#include "timer3.h"
#include "timer4.h"
#include "utils.h"
#include "gps.h"
#include "log.h"
#include "led.h"
#include "irq.h"
#include "dac.h"
#include "pll.h"



/**
 * ���� ���� ���
 */
#pragma section("FLASH_code")
bool test_adc(void *in)
{
    DEV_STATUS_STRUCT *status;
    ADS1282_Params par;
    ADS1282_ERROR_STRUCT err;
    bool res = false;
    int i;
    u8 l1, l2, l3, l4;

    LED_get_state(&l1, &l2, &l3, &l4);

    do {
	if (in != NULL) {
	    status = (DEV_STATUS_STRUCT *) in;
	    status->st_main |= 0x10;	// ����������� ���������� 
	    status->st_adc = 0;	// �� �����������

	    par.mode = TEST_MODE;	/* �������� ����� */
	    par.res = 1;		/* ����������������� */
	    par.chop = 1;
	    par.sps = SPS500;
	    par.pga = PGA2;
	    par.hpf = 0;

	     /* ���� �� ������  */
	    if (ADS1282_config(&par) == false) {
		ADS1282_get_error_count(&err);
		for (i = 0; i < ADC_CHAN; i++) {
		    if (err.adc[i].cfg0_wr != err.adc[i].cfg0_rd || err.adc[i].cfg1_wr != err.adc[i].cfg1_rd)
			status->st_adc |= (1 << i);	// � ����� ������ ������
		}
		break;
	    }

	    ADS1282_start();	/* ��������� ��� � PGA  */
	    ADS1282_start_irq();	/* ��������� IRQ  */
	    delay_ms(250);	/* �������� ����� ���������� IRQ*/
	    ADS1282_stop();	/* ���� ��� */

	    /* ������� ����������  */
	    i = ADS1282_get_irq_count();


/* ���� �� �������� �� 8 ��� ������ - �����������!!!  */
#if QUARTZ_CLK_FREQ==(19200000)
	    if (i < 50) {
		break;
	    }
#endif
	    res = true;
	    status->st_main &= ~0x10;	// ����������� �������� 
	}
    } while (0);
    LED_set_state(l1, l2, l3, l4);
    return res;
}



/**
 * ���� EEPROM, ���������� ��������� � ������������� ���� � �������
 */
#pragma section("FLASH_code")
bool eeprom_test(void *par)
{
    DEV_STATUS_STRUCT *status;
    bool res = false;
    u8 l1, l2, l3, l4;

    LED_get_state(&l1, &l2, &l3, &l4);


    if (par != NULL) {
	status = (DEV_STATUS_STRUCT *) par;
	status->st_test1 |= 0x08;	/* ������ ������ � eeprom */

	status->eeprom = read_all_data_from_eeprom();	/* �������� ��� ������ � eeprom  */

	if (status->eeprom == 0) {
	    status->st_test1 &= ~0x08;	/* ��� ������ */
	    res = true;
	}
    }
    LED_set_state(l1, l2, l3, l4);
    return res;
}


/**
 * ���� ����� RTC.
 * ����� / ������ ����� �� RTC, ���� ������ - �� ���������� 
 * ��������� ������� - ��� ��������, ��� ���� ����������  
 * ����� ���������� ������ ���� �� �������� �������
 */
#pragma section("FLASH_code")
bool rtc_test(void *par)
{
    DEV_STATUS_STRUCT *status;
    int t0, t1, tc;
    int diff;
    bool res = false;
    u8 l1, l2, l3, l4;

    LED_get_state(&l1, &l2, &l3, &l4);

    do {
	diff = 10 * 3;		/* �� 30 ������ ������, ����� ����� */
	tc = get_comp_time();	/* ����� ���������� */

	/* ���� 1.5 ������� */
	t0 = get_msec_ticks();
	while (get_msec_ticks() - t0 < 1500) {
	    LED_test();
	}

	t0 = get_rtc_sec_ticks();	/* �������� ������� */

	/* ���� ������ ����� */
	if (t0 < 0)
	    break;

	set_rtc_sec_ticks(t0 + diff);	/* ����� ����� ������ */


	t1 = get_rtc_sec_ticks();	/* �������� ������� ����� */

	if (par != NULL) {
	    status = (DEV_STATUS_STRUCT *) par;
	    status->st_test0 |= 0x01;	/* ������������� �����  */
	    status->temper1 = 0;	/* ����� ��������� ������ */
	    status->press = 0;

	    /* ���� �� ���������� � �������� ������� */
	    if (abs(t1 - t0) < (diff + 5) && tc - 3600 * 24 < t0) {
		status->st_test0 &= ~0x01;	/* ������������� ����� - ����� */
		status->st_main &= ~0x01;	/* ������� "��� �������"  */
		res = true;
	    }
	}
    } while (0);

    if (t0 > 0)
	set_rtc_sec_ticks(t0);	/* ����� ����� */
    LED_set_state(l1, l2, l3, l4);
    return res;
}


/**
 * Test ������ ����������� � ��������:  ��������� ������������ �� bmp085 
 */
#pragma section("FLASH_code")
bool test_bmp085(void *par)
{
    DEV_STATUS_STRUCT *status;
    bool res = false;
    int p, t;
    u8 l1, l2, l3, l4;

    LED_get_state(&l1, &l2, &l3, &l4);

    if (par != NULL) {
	status = (DEV_STATUS_STRUCT *) par;
	status->st_test0 |= 0x02;	/* ������ ������� ����������� � �������� */

	if (bmp085_init() && bmp085_data_get(&t, &p)) {
	    status->st_test0 &= ~0x02;	/* ������� ������ ������� */
	    res = true;
	}
    }
    LED_set_state(l1, l2, l3, l4);
    return res;
}

/**
 * Test �������
 */
#pragma section("FLASH_code")
bool test_cmps(void *par)
{
    DEV_STATUS_STRUCT *status;
    lsm303_data data;
    bool res = false;
    u8 l1, l2, l3, l4;
    int t0;

    LED_get_state(&l1, &l2, &l3, &l4);

    do {
	if (par == NULL)
	    break;

	status = (DEV_STATUS_STRUCT *) par;
	status->st_test0 |= 0x08;


	/* ���� 2.5 ������� */
	t0 = get_msec_ticks();
	while (get_msec_ticks() - t0 < 2500) {
	    LED_test();
	}


	if (lsm303_get_comp_data(&data) == true) {

	    /* ��� ������ ������� ����� �� ����� ���� == 0 */
	    if ((data.x == 0 && data.y == 0 && data.z == 0) || (data.x == -1 && data.y == -1 && data.z == -1))
		break;

	    status->st_test0 &= ~0x08;
	    res = true;
	}
    } while (0);
    LED_set_state(l1, l2, l3, l4);
    return res;
}


/**
 * Test ������������� - ���� ���������� � ��������!
 */
#pragma section("FLASH_code")
bool test_acc(void *par)
{
    DEV_STATUS_STRUCT *status;
    lsm303_data data;
    bool res = false;
    u8 l1, l2, l3, l4;
    int t0;

    LED_get_state(&l1, &l2, &l3, &l4);

    do {
	if (par != NULL)
	    break;

	status = (DEV_STATUS_STRUCT *) par;
	status->st_test0 |= 0x04;

	/* ���� 2.5 ������� */
	t0 = get_msec_ticks();
	while (get_msec_ticks() - t0 < 2500) {
	    LED_test();
	}

	/* ��� ������ ������������� ����� � ����������� �� ����� ���� == 0 */
	if (lsm303_get_acc_data(&data) == true) {

	    if ( /*(data.x == 0 && data.y == 0 && data.z == 0) || */ (data.x == -1 && data.y == -1 && data.z == -1))
		break;

	    status->st_test0 &= ~0x04;
	    res = true;
	}
    } while (0);
    LED_set_state(l1, l2, l3, l4);
    return res;
}


/**
 * ���� GPS. � ������� 5-�� ������ �������� �������� NMEA
 */
#pragma section("FLASH_code")
bool test_gps(void *par)
{
    DEV_STATUS_STRUCT *status;
    bool res = false;
    s64 t0;
    u8 l1, l2, l3, l4;

    LED_get_state(&l1, &l2, &l3, &l4);

    do {
	if (par == NULL)
	    break;

	status = (DEV_STATUS_STRUCT *) par;
	status->st_test0 |= 0x10;	// ������

	/* ������� ������� */
	t0 = get_msec_ticks();
	do {
	    LED_test();
	} while (get_msec_ticks() - t0 < 500);
	gps_set_grmc();

	/* ���� ���� ����� �� ������ ���������� */
	t0 = get_msec_ticks();
	do {
	    res = gps_nmea_exist();	/* ������ ��� ������ 1 GSA */
	    if (res) {
		status->st_test0 &= ~0x10;
		break;
	    }
	    LED_test();
	} while (get_msec_ticks() - t0 < 4500);

    } while (0);
    LED_set_state(l1, l2, l3, l4);
    return res;
}

/**
 * ���������� ������� ������
 */
#pragma section("FLASH_code")
void test_reset(void *par)
{
    DEV_STATUS_STRUCT *status;
    int res;
    u8 l1, l2, l3, l4;

    LED_get_state(&l1, &l2, &l3, &l4);

    if (par != NULL) {
	status = (DEV_STATUS_STRUCT *) par;

	// �������� ������� ������
	res = read_reset_cause_from_eeprom();
	if (res == CAUSE_POWER_OFF) {
	    status->st_reset = 1;
	} else if (res == CAUSE_EXT_RESET) {
	    status->st_reset = 2;
	} else if (res == CAUSE_BROWN_OUT) {
	    status->st_reset = 4;
	} else if (res == CAUSE_WDT_RESET) {
	    status->st_reset = 8;
	} else if (res == CAUSE_NO_LINK) {
	    status->st_reset = 16;
	} else {
	    status->st_reset = 32;	// �������������� �����
	}
    }
    LED_set_state(l1, l2, l3, l4);
}


/**
 * ����������� DAC 4 � ���������
 * ������ ����������� � ����� ������ ������. �� �����!
 */
#pragma section("FLASH_code")
bool test_dac4(void *par)
{
    DEV_STATUS_STRUCT *status;
    int i;
    int min = 0, max = 0;
    u64 t0, t1;
    bool res = false;
    u8 l1, l2, l3, l4;

    LED_get_state(&l1, &l2, &l3, &l4);

#if QUARTZ_CLK_FREQ==(19200000)
    do {
	if (par == NULL)
	    break;

	status = (DEV_STATUS_STRUCT *) par;

	status->st_test1 |= 0x20;	// ������ ���������� 4 ���
	status->st_test1 |= 0x80;	// ������ ������� T3


	DAC_write(DAC_4MHZ, 0);	// ������ �� dac4 0

	TIMER3_init();		// �������� 3-� ������. ������ �� IRQ �� ������!
	IRQ_register_vector(TIM1_TIM3_VECTOR_NUM);

	/* ���� ���� ����� �� ������ ���������� */
	t0 = get_msec_ticks();
	while (get_msec_ticks() - t0 < 1500) {
	    LED_test();
	}

	/* ���� 2.5 ������� */
	t0 = get_msec_ticks();
	t1 = t0;
	while (t1 - t0 < 4500) {
	    t1 = get_msec_ticks();
	    LED_test();
	    if (TIMER3_get_counter() > 2000000 && TIMER3_get_counter() < 2040000) {
		res = true;
		break;
	    }
	}
	if (res == false)
	    break;

	min = TIMER3_get_period();

	DAC_write(DAC_4MHZ, 0x3ff0);	// ������ �� dac4 ��������
	res = false;

	/* ���� ���� ����� �� ������ ���������� */
	t0 = get_msec_ticks();
	while (get_msec_ticks() - t0 < 1500) {
	    LED_test();
	}

	/* ���� 1.5 ������� */
	t0 = get_msec_ticks();
	t1 = t0;
	while (t1 - t0 < 4500) {
	    t1 = get_msec_ticks();
	    LED_test();
	    if (TIMER3_get_counter() > 2000000 && TIMER3_get_counter() < 2040000) {
		res = true;
		break;
	    }
	}
	if (res == false)
	    break;

	max = TIMER3_get_period();
	status->st_test1 &= ~0x80;	// ������� ������ ������� 3

	if (min > 0 && max > 0) {
	    status->st_test1 &= ~0x20;	// ������� ������ ���������� 4.096 ���
	    res = true;
	}

    } while (0);
    status->st_tim3[0] = min;
    status->st_tim3[1] = max;

    IRQ_unregister_vector(TIM1_TIM3_VECTOR_NUM);
    TIMER3_disable();

#elif QUARTZ_CLK_FREQ==(8192000)
    do {
	if (par == NULL)
	    break;

	status = (DEV_STATUS_STRUCT *) par;
	status->st_test1 &= ~0x20;	// ������ ���������� 4 ��� - �����
	status->st_test1 &= ~0x80;	// ������ ������� T3 - �����
	res = true;
    } while (0);

    status->st_tim3[0] = SCLK_VALUE - 100;
    status->st_tim3[1] = SCLK_VALUE + 100;
#endif
    LED_set_state(l1, l2, l3, l4);
    return res;
}


/**
 * ����������� DAC 19 � ���������
 * ������ ����������� � ����� ������ ������. �� �����!
 */
#pragma section("FLASH_code")
bool test_dac19(void *par)
{
    u64 t0, t1;
    DEV_STATUS_STRUCT *status;
    int min = 0, max = 0;
    bool res = false;
    bool ch1 = false, ch0 = false;
    u8 l1, l2, l3, l4;

    LED_get_state(&l1, &l2, &l3, &l4);


    do {
	if (par == NULL)
	    break;

	status = (DEV_STATUS_STRUCT *) par;
	status->st_test1 |= 0x10;	// ������ ���������� 19 ���
	status->st_test1 |= 0x40;	// ������ ������� T4

	DAC_write(DAC_19MHZ, DAC19_MIN_DATA);	// ������ �� dac 0

	TIMER4_config();	// ������� ������� ������
	TIMER4_enable_irq();
	TIMER4_del_vector();	// ������ ������ - ����� �� ������ �������� ������


	status->st_test1 |= 0x01;	// GPS �������

	/* ���� ���� ����� �� ������ ���������� */
	t0 = get_msec_ticks();
	while (get_msec_ticks() - t0 < 1500) {
	    LED_test();
	}


	t0 = get_msec_ticks();
	while (get_msec_ticks() - t0 < 4500) {
	    ch0 = TIMER4_wait_for_irq();
	    if (get_msec_ticks() - t0 > 2500 && ch0 == true) {
		delay_ms(20);
		min = TIMER4_get_period();
		break;
	    }
	    LED_test();
	}


	status->st_test1 &= ~0x40;	// ������� ������ ������� T4 - ��� PPS


	t0 = get_msec_ticks();
	DAC_write(DAC_19MHZ, DAC19_MAX_DATA);	// ������ �� dac max
	do {
	    t1 = get_msec_ticks();
	    ch1 = TIMER4_wait_for_irq();
	    if (t1 - t0 > 2500 && ch1 == true) {
		delay_ms(20);
		max = TIMER4_get_period();
		break;
	    }
	    LED_test();
	} while (t1 - t0 < 4500);

	/* ������� ���� ��� �������� */
	if (min == 0 || ch0 == false || min >= SCLK_VALUE || ch1 == false || max <= SCLK_VALUE) {
	    res = false;
	    break;
	}


	/* ������� ������ ���������� 19.2 ��� */
	if (abs(max - min) > 20) {
	    status->st_test1 &= ~0x10;
	    res = true;
	}

    }
    while (0);

    // ��� ���������
    status->st_tim4[0] = min;
    status->st_tim4[1] = max;

    TIMER4_disable();		/* ��������� ������� ������ */
    TIMER4_disable_irq();
    LED_set_state(l1, l2, l3, l4);
    return res;
}



/**
 * ����������� ��� ������
 */
#pragma section("FLASH_code")
void test_all(void *par)
{
    DEV_STATUS_STRUCT *status;
    int t0, i;

    /* �� ���� ������� UART0 */
    if (par != NULL && gps_init() == 0) {
	status = (DEV_STATUS_STRUCT *) par;
	status->st_main |= 0x80;	/* ���� ������������ (� ������) */

	status->st_test1 |= 0x01;	/* GPS ������� */

	adc_init(status);	/* ������������� ����������� �������� � ��. �� I2C */

	test_gps(status);	/* Test GPS ������ */
	DAC_init();		/* ��� ���� */

	eeprom_test(status);	/* ���� eeprom  */
	test_reset(status);	/* �������� ������� ������ */

	/* Test RTC: ��������� ����� �� RTC, ���� ������ - �� ���������� */
	for (i = 0; i < 3; i++) {
	    if (rtc_test(status) == true) {
		t0 = get_rtc_sec_ticks();	/* ���� ����� - �������� �����  */
		break;
	    } else {
		t0 = get_comp_time();	/* ����� ���������� */
		delay_ms(50);
	    }
	}

	/* ���� test-dac4 ����� ... - ������? */
	test_bmp085(status);	/* Test ������ ����������� � ��������:  ��������� ������������ �� bmp085 */
	test_acc(status);	/* Test �������������: */
	test_cmps(status);	/* Test �������: */
	test_adc(status);	/* ���� ��� */

	for (i = 0; i < 3; i++) {
	    if (test_dac19(status))	/* ���� �������4 � ���������� 19.2 ��� */
		break;
	}
	test_dac4(status);	/* ���� �������3 � ���������� 4.096 ��� */


	t0 = get_rtc_sec_ticks();	/* ���� ����� - �������� �����  */
	set_sec_ticks(t0);	/* ��������� ���� */
    }
    status->st_main &= ~0x80;	/* ������� ������ "������������" */
    status->st_test1 &= ~0x01;	/* GPS �������� */
    gps_close();		/* ��������� UART0  */
}
