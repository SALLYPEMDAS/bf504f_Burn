/******************************************************************************************   
 * ��������� - ����������, ����� �� ������������ ������,
 * ������� ����������� ������ � ������ ����������
 *****************************************************************************************/
#include "utils.h"
#include "power.h"
#include "main.h"
#include "rele.h"
#include "irq.h"
#include "led.h"
#include "pll.h"
#include "eeprom.h"
#include "log.h"

#define BUTPOW_PUSHED		PG15
#define POWER_VECTOR_NUM	POW_TIM5_VECTOR_NUM
#define BOUNCE_TIMER_PERIOD	TIMER_PERIOD	/* 1 � */

/******************************************************************************************
 * ����������� ������� ��� �����������
 *******************************************************************************************/
static inline void inner_timer_start(void);
static inline void inner_timer_stop(void);

/* �� �������� ���������� */
static struct {
    int  tick;
    u8   req_cnt;
    u8   power_led;
    bool init_ok;
} BOUNCE_POWER_STRUCT;


/* ��������� ������� �� ���������, ������ 2 ��� ������ ��������!*/
#pragma section("FLASH_code")
int POWER_on(void)
{
    int rst;

    LED_on(LED_POWER);		/* �������� ��������� */
    delay_ms(150);
    RELE_on(RELEPOW);		/* �������� ����-� ���������� ���� ��� ���������!!! */
    LED_off(LED_POWER);		/* �������� ��������� */

    rst = read_reset_cause_from_eeprom();	/* ������� ������ */
    delay_ms(50);

#if !defined	JTAG_DEBUG   /* �� ��������� ��� ����������� �� ������ */
    /*  ����������� ����-����������, ����� ������ ������!   */
    if (rst != CAUSE_EXT_RESET && rst != CAUSE_WDT_RESET) {

	/* ��������� ������� ��� ����� PG15 � ������ ��� �� ���� - ��� ����� �������� ��� 1 ��� �������� � 0 */
	*pPORTG_FER &= ~BUTPOW_PUSHED;
	*pPORTGIO_DIR &= ~BUTPOW_PUSHED;
	*pPORTGIO_INEN |= BUTPOW_PUSHED;
	*pPORTGIO_POLAR |= BUTPOW_PUSHED;
	*pPORTGIO_EDGE &= ~BUTPOW_PUSHED;
	ssync();


	/* ���� ���������� ������ � ������� 3-� ������  */
	if (!(*pPORTGIO & BUTPOW_PUSHED)) {
	    POWER_off(0, false);	// �� ���������� ������� reset
	    while (1);		/*  ����������� ����-����������, ����� ������ ������!   */
	}
    }
#endif
    RELE_off(RELEBURN);		/* ��������� ���� �������� */

    return rst;			/* ������� ������ ��� ���������� ���������� */
}



/* ���������� ������� ������� */
#pragma section("FLASH_code")
void POWER_off(int cause, bool modem)
{
  if(cause != 0) {
      log_write_log_file("INFO: Power Off\n");
      write_reset_cause_to_eeprom(cause);	/* ������� ������� ������ */
      print_reset_cause(cause);
      write_work_time();			/* �������� ����� ������ */
   }

    if (modem) {
	log_write_log_file("INFO: Power Off Modem\n");
	log_close_log_file();
	RELE_off(RELEAM);	/* ����� ��������� ����� */
    }



#if !defined	JTAG_DEBUG
    RELE_off(RELEPOW);
#endif

/*  ����������� ���� - ����������, ����� ������ ������! ������ ���� ����� ���� ����������� ������.*/
   do {
      LED_toggle(LED_POWER);		
      delay_ms(250);
    } while (1);
}



/**
 * ������������� ��� ���������-���������� ������� 
 * ����� PORTG - �� ������! 
 */
#pragma section("FLASH_code")
void POWER_init(void)
{
    IRQ_unregister_vector(POWER_VECTOR_NUM);	/* �������������� ���������� */
    LED_off(LED_POWER);

    /* ��������� ������� ��� ����� PG15 - ������ �� �� ���� */
    *pPORTG_FER &= ~BUTPOW_PUSHED;
    *pPORTGIO_DIR &= ~BUTPOW_PUSHED;
    *pPORTGIO_INEN |= BUTPOW_PUSHED;
    *pPORTGIO_POLAR |= BUTPOW_PUSHED;
    *pPORTGIO_EDGE &= ~BUTPOW_PUSHED;
    ssync();

    /* ���������� ��������� �� ������  */
    *pPORTGIO_POLAR |= BUTPOW_PUSHED;

    /* ��������� ���������� �� �������: ����� A ��� ����� ����� */
    *pPORTGIO_MASKA_SET |= BUTPOW_PUSHED;

    /* � ����������������� ��� �� �� 13 ��������� */
    *pSIC_IAR5 &= 0xFFFFFFF0;
    *pSIC_IAR5 |= 0x00000006;
    *pSIC_IMASK1 |= IRQ_PFA_PORTG;
    ssync();

    /* ������� ������ ��������������, �� �� ��������� + ������ ������ 
     * ������ �� BOUNCE_TIMER_PERIOD, ����������  */
    *pTIMER5_CONFIG = PERIOD_CNT | IRQ_ENA | PWM_OUT;
    *pTIMER5_PERIOD = BOUNCE_TIMER_PERIOD / 10;	/* �� 100 ms */
    *pTIMER5_WIDTH = BOUNCE_TIMER_PERIOD / 10000;	/* 0.1 ms */
    ssync();

    BOUNCE_POWER_STRUCT.req_cnt = 0;
    BOUNCE_POWER_STRUCT.tick = 20;

    /* ������������ ���������� �� IVG13 */
    IRQ_register_vector(POWER_VECTOR_NUM);

    /* ����������������� TIMER5 �� ��������� IVG13 */
    *pSIC_IMASK1 |= IRQ_TIMER5;
    *pSIC_IAR4 &= 0xFF0FFFFF;
    *pSIC_IAR4 |= 0x00600000;
    ssync();
}


/* ����������, ��� ��������� ��� ���������� �������� */
#pragma section("FLASH_code")
void POWER_MAGNET_ISR(void)
{
    BOUNCE_POWER_STRUCT.power_led = LED_get_power_led_state();	/* ��������� ��������� ������� ����� */
    inner_timer_start();	/* ��������� ������ �� ���� */
    *pPORTGIO_MASKA_CLEAR = BUTPOW_PUSHED;	/* ���������� ���������� � ����� ����� */
    *pPORTGIO_CLEAR = BUTPOW_PUSHED;
}


/* ������ ��������� �������. ���� ������ �� ������� - ���������� ������� ������ */
#pragma section("FLASH_code")
void BOUNCE_TIMER_ISR(void)
{
    /* ���������� ���������� - ���� ����� �� ���������  */
    *pTIMER_STATUS = TIMIL5;
    ssync();

    BOUNCE_POWER_STRUCT.tick++;	// �������� �������

    /* ���� �� ������� ���� ������������ - ������������� ��� �������� ������� */
    if (!(*pPORTGIO & BUTPOW_PUSHED)) {
	inner_timer_stop();	/* ������������� ������� ���������� ������ ���� ������ ������ */
	*pPORTGIO_MASKA_SET = BUTPOW_PUSHED;
	ssync();
    }
    /* ����� 1.5 ������� ����� ����� ���������� � �������� - ����������, 
     * ���� ����� ��������� ���������, �������� ����� �� ������� � ����������
     ** ��������� � ����� ������ ���������
     */
    if (BOUNCE_POWER_STRUCT.tick == 15 && *pPORTGIO & BUTPOW_PUSHED) {
	LED_set_power_led_state(BOUNCE_POWER_STRUCT.power_led);
	send_magnet_request();	/* ������ �� ���������� ��� ������������ */
    }
}

/** 
 * ����������� 5-� ������, ������ 100 �� (TIMER_PERIOD / 10)
*/
#pragma section("FLASH_code")
static inline void inner_timer_start(void)
{
    *pTIMER_ENABLE = TIMEN5;	/* ��������� */
    ssync();

    /* ������ ������� */
    if (BOUNCE_POWER_STRUCT.req_cnt)
	LED_set_power_led_state(LED_ON_STATE);
    BOUNCE_POWER_STRUCT.req_cnt++;
}

/**
 * ������������� ���� 
 */
#pragma section("FLASH_code")
static inline void inner_timer_stop(void)
{
    int state;

    *pTIMER_DISABLE = TIMEN5;	/* ������ ������ */
    ssync();
    state = get_dev_state();


   /* ���������� ����� ��� ���� --- ��������� �� ������� ���� �� ������ */
    if (state == DEV_ERROR_STATE || state == DEV_CHOOSE_MODE_STATE)
	LED_set_power_led_state(LED_QUICK_STATE);
    else
	LED_set_power_led_state( /*BOUNCE_POWER_STRUCT.power_led */ LED_OFF_STATE);	

    BOUNCE_POWER_STRUCT.tick = 0;	/* ������� ��������� */
    BOUNCE_POWER_STRUCT.init_ok = false;
}
