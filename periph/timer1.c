/* ������ 1 ������ �� ����� ������� ���������������� ����������!
 * ����� ���������, ���� ������� ��������� ������ ������� GNS
 */
#include "timer1.h"
#include "gps.h"
#include "irq.h"

#define	TIM1_MAX_ERROR		(1000)

/************************************************************************
 * 	����������� ����������
 ************************************************************************/

/* ������� ��������� + ����� ������������� */
static volatile struct {
    u32 sec;			/* ������� */
    void (*call_back_func) (u32);	/* ��������� �� �������, ������� ����� �������� �� ���������� �������  */
    u16  check_err;		/* ��������� ������� ������������ */
    bool sync_ok;		/* ������ ��������� OK */
    bool run_ok;		/* ������ ���������� OK */
} timer_tick_struct;


/**
 * ������ ��������� ������ ����� ���������, ������ �� �������� � ������ �����, 
 * � �������� ��������� �������� ��������� �� ��� ���������
 * ������ ����� ������� �������� ENABLE!!!
 */
#pragma section("FLASH_code")
int TIMER1_config(void)
{
    /* ����� ���, ������� �� PERIOD, ����� �������, ���������� ��� � ������� + �������� ������ */
    *pTIMER1_CONFIG = PWM_OUT | PERIOD_CNT | IRQ_ENA /*| PULSE_HI */ ;

    /* �������� ��������� */
    timer_tick_struct.sec = 0;
    timer_tick_struct.check_err = 0;
    timer_tick_struct.call_back_func = NULL;	/* �� �� ��������, ���� �� ��������� */
    timer_tick_struct.sync_ok = false;
    timer_tick_struct.run_ok = false;

    /* ������� �����:
     * 1 ������������ - ��� SCLK_VALUE / TIMER_US_DIVIDER! 
     * 1 ������������ SCLK_VALUE / TIMER_MS_DIVIDER
     * 1 ������� ��� SCLK_VALUE
     */
    *pTIMER1_PERIOD = TIMER_PERIOD;	/* 1 ������� */
    *pTIMER1_WIDTH = 202;	/* �.�. TIMER_PERIOD / TIMER_US_DIVIDER * 4.225 --- 202 */

    IRQ_register_vector(TIM1_TIM3_VECTOR_NUM);
    TIMER1_enable_irq();	/* ����� �� ��� �������� */

    return 0;			/* ��� ��  */
}

/* ������ ��� ����������! */
#pragma section("FLASH_code")
void TIMER1_quick_start(long t)
{
    TIMER1_config();		/* ���������� ��������� ������  */
    TIMER1_set_sec(t);
    timer_tick_struct.sync_ok = true;	/* ��� ������������� */
    TIMER1_enable();		/* � ��������� */
}

#pragma section("FLASH_code")
u32 TIMER1_get_error(void)
{
    return (u32)timer_tick_struct.check_err;
}


/**
 * Timer1 ISR �������� ������ �������!
 * � ���� ������� �������� ��������� � ������ ��� ������ ���������
 * ����� �� ���� ������ ������ ��������� IRQ � ��.
 * ������ TIMER4 ������� �������!
 * ������ ���� �� �������, � ����� ������ � ������� 4 - ��� ��������� ������ � ����!!!
 */
section("L1_code")
void TIMER1_ISR(void)
{
    /* ���������� ���������� - ����� ����� ���� ���������� ������ */
    *pTIMER_STATUS = TIMIL1;
    ssync();

    /* ������ ���� �� ��������! */
    if (!timer_tick_struct.sync_ok) {
	timer_tick_struct.sync_ok = true;
	timer_tick_struct.run_ok = false;
	*pTIMER1_PERIOD = TIMER_PERIOD;	/* 1 ������� */
	ssync();
    }

    timer_tick_struct.sec++;	/* ������� ������ */

    /* �������� callback, ���� �� ���������� */
    if (timer_tick_struct.call_back_func != NULL)
	timer_tick_struct.call_back_func(timer_tick_struct.sec);
}



/**
 * ���������� ��������� �������, ���������� ������� ���� ������ �������,
 * ����� ��� ���� �� ��� ���������� 
 * ��� ��� �� �������� ��� ������ 1 �������!
 */
section("L1_code")
void TIMER1_set_sec(u32 sec)
{
    timer_tick_struct.sec = sec;
    timer_tick_struct.run_ok = true;
}


/**
 *  �������� ������� ������ �� ������ ������� 
 */
section("L1_code")
u32 TIMER1_get_sec(void)
{
    return timer_tick_struct.sec;
}

/**
 * �������� ������� ����� �� ������� � ������������ 
 * return: s64.  PS: ��� ����� �� ������� 32 �������� �� �����
 * ���� ����� �������� � "���������" ������ - ������� 0
 */
section("L1_code")
s64 TIMER1_get_long_time(void)
{
    register u32 cnt0, sec, cnt1;

    cnt0 = *pTIMER1_COUNTER;
    ssync();

    sec = timer_tick_struct.sec;
    cnt1 = *pTIMER1_COUNTER;
    ssync();

    /* ���� ����� ����� � ����� ������ ������������ */
    if ((int) cnt1 < (int) cnt0) {
	timer_tick_struct.check_err++;	// �������, ��� ��� �� ����� ������� ������
	sec = timer_tick_struct.sec;	// ��� ��� ���������!
	cnt1 = *pTIMER1_COUNTER;
	ssync();
    }

    return ((u64) sec * TIMER_NS_DIVIDER + (u64) cnt1 * TIMER_MS_DIVIDER / TIMER_FREQ_MHZ);
}



/**
 * ���������� �������, ��� ��� �� �������� ��� ������ 1 �������! 
 */
section("L1_code")
void TIMER1_set_callback(void *ptr)
{
    timer_tick_struct.call_back_func = (void (*)(u32)) ptr;
}


/**
 * ������ callback ������� 
 */
#pragma section("FLASH_code")
void TIMER1_del_callback(void)
{
    timer_tick_struct.call_back_func = NULL;
}

/**
 * ������ ����, ������� � ���������������? 
 */
section("L1_code")
bool TIMER1_is_run(void)
{
    bool res;
    res = (*pTIMER_STATUS & TRUN1) ? true : false;
    res &= (timer_tick_struct.sync_ok);
    res &= (timer_tick_struct.run_ok);
    return res;
}


/**
 * ���� ����� callback
 */
section("L1_code")
bool TIMER1_is_callback(void)
{
    return ((timer_tick_struct.call_back_func == NULL) ? false : true);
}


/**
 * ������ ���������� ������� 1-�� �������
 */
section("L1_code")
u32 TIMER1_get_counter(void)
{
    register u32 cnt;

    cnt = *pTIMER1_COUNTER;
    ssync();
    return cnt;
}


/**
 * ������� ��������� PPS � ����� ������� 1. ��� �������� � �� = ticks * 20.8333333
 * ������! ������ 4 ������� �� �� SCLK_VALUE, � �� ���� ������ (SCLK_VALUE +- 50)!
 * �������� ���� �� �������� ����� ��������� TIMER4_PERIOD
 * � t1 � t4 ��������� �������
 */
section("L1_code")
int TIMER1_get_drift(u32 * pT1, u32 * pT4)
{
    register long t1, t4;
    long drift0, drift1;
    s64 temp;

    /* ���� ������ �� �������  */
    if (!(*pTIMER_STATUS & TRUN4))
	return -1;

    t1 = *pTIMER1_COUNTER;
    t4 = *pTIMER4_COUNTER - 7;

    /* �������� � ������� */
    if (pT1 != NULL && pT4 != NULL) {
	*pT1 = t1;
	*pT4 = t4;
    }

    /* �� ������� � ������ ������������ - ������� �� ������ */
    drift0 = t1 - t4;
    if (t1 < t4)
	t1 += TIMER_PERIOD;
    else
	t4 += TIMER_PERIOD;

    drift1 = t1 - t4;

    /* ����� ����� ��������� � ����������� */
    temp = abs(drift0) < abs(drift1) ? drift0 : drift1;
    temp = (temp * TIMER_MS_DIVIDER) / TIMER_FREQ_MHZ;	/* ��������� � �� */

    return temp;
}

/**
 * �������� ���� ������ 1 ��������� ������ �������� �����.
 * PPS->|__xxxxxxxxxx__NMEA__________________________________|<-PPS
 * ������ ���� ^ ����� (��� xxx - ����� �������� NMEA) ����� ����� ���������� �����
 */
section("L1_code")
int TIMER1_sync_timer(void)
{
    int sec;

    while (TIMER1_get_counter() > TIM1_MAX_ERROR) {
	ssync();
    }

    sec = gps_get_nmea_time() + 1;	/* ��� ������ ������� - �������� � ���� ����� �� NMEA + ������� */
    TIMER1_set_sec(sec);	/* ����� ������������� ����� � ������ */

    return sec;
}
