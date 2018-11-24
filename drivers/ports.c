#include "xpander.h"
#include "main.h"
#include "ports.h"
#include "timer2.h"
#include "timer3.h"
#include "sport0.h"
#include "utils.h"
#include "rele.h"
#include "spi1.h"
#include "led.h"



/**
 * ������������� ������ �� �� + sport0 � SPI1
 * ��-��������� ��� ����� �� ����� � Z - ��������� (page 9 - 3)
 * ������ �������� ��� �� �������
 * 
 */
#pragma section("FLASH_code")
void init_bf_ports(void)
{
    /* ����� F: ��������� ������� � ��� �� ���� */
    *pPORTF_FER = 0;
    *pPORTFIO_DIR = 0;

    /* �� ���� ������ ��������� ������� */
    *pPORTG_FER &= ~(PG0 | PG1 | PG2 | PG3 | PG4 | PG5 | PG6 | PG7 | PG8 | PG9 | PG10 | PG11 /* | PG12 | PG13 */  | PG14 | PG15);
    *pPORTGIO_CLEAR = (PG3 | PG4 | PG11);	/* ������ � ���� */
    *pPORTGIO_DIR = (PG3 | PG4 | PG11);	/* �� �����, ��� ��������� �� ����  */
#if 0
    *pPORTH_FER &= ~(PH0 | PH1 | PH2);	/* ����� H: PH1 �� ����-��������� �� ����� � � ���� */
    *pPORTHIO_CLEAR = (PH0 | PH2);	/* ������ � ���� */
    *pPORTHIO_DIR = (PH0 | PH2);
#else
    *pPORTH_FER &= ~(PH1 | PH2);	/* ����� H: PH1 �� ����-��������� �� ����� � � ���� */
    *pPORTHIO_CLEAR = PH2;	/* ������ � ���� */
    *pPORTHIO_DIR = PH2;
#endif
    TIMER2_init();		/* ��������� ��������������� ������ - ����� ������! � ���� �������!!! */
    SPI1_init();		/* ������������ �� SPI1 ���������� */
    SPORT0_config();		/* �����0  */
}

/**
 * ������������� ������ �� ������
 * ��-��������� ��� ����� �� ����� � Z - ��������� (page 9 - 3)
 */
#pragma section("FLASH_code")
void init_atmega_ports(void)
{
#if 0
    /* ������� � ������ reset � ���������� PH0 */
    *pPORTH_FER &= ~PH0;	/* ��������� ������� */
    *pPORTHIO_CLEAR = PH0;	/* ������ 0 �� ����� */
    *pPORTHIO_DIR |= PH0;	/* ������ �� �� ����� */
    ssync();
    *pPORTHIO_SET = PH0;	/* ������ 1 �� ����� */
    ssync();
#endif

    delay_ms(WAIT_START_ATMEGA);	/* ��������, �.�. ��� DSP ����� ������� - �� �������� ���������������� Atmega ��������!!! */


    pin_clr(WUSB_EN_PORT, WUSB_EN_PIN);	/* ����������� ����� B - ������ WUSB_EN_PIN � MISO ��� ������, ��������� � Z */
    pin_clr(USBEXT33_EN_PORT, USBEXT33_EN_PIN);	/* ����������� ����� - ��� Z ����� ����� USBEXT33 */
    pin_clr(GATEBURN_PORT, GATEBURN_PIN);	/* ����������� ����� A - ��� � Z - ����� GATEBURN (�� ��� 0) ������� ���� �������� */
    pin_set(USB_EN_PORT, USB_EN_PIN);	/* ����� D � Z ���������, ����� USB_EN_PIN  � GPS_EN_PIN */
    pin_set(UART_SELA_PORT, UART_SELA_PIN);	/* ���������� ����� E � z ��������� ����� ���������, �� ����� */
    pin_clr(APWR_EN_PORT, APWR_EN_PIN);	/* ��������� �����  - ����� �� ��������!!! */

    pin_clr(EXP_MISO_PORT, EXP_MISO_PIN);
    pin_clr(GPS_EN_PORT, GPS_EN_PIN);
    pin_set(SD_SRCSEL_PORT, SD_SRCSEL_PIN);
    pin_set(FT232_RST_PORT, FT232_RST_PIN);	/* 1 �� FT232 */
    pin_set(MES_MUX_SELA_PORT, MES_MUX_SELA_PIN);	/* �� ����� */
    pin_set(MES_MUX_SELB_PORT, MES_MUX_SELB_PIN);	/* �� ����� */

    /* ����� �����. Obscaja castj */
    LED_init();			/* �������, ������ ����������� ��������� � SPI */
    RELE_init();		/* ����. ���� ���� */
}


#pragma section("FLASH_code")
static void set_status_bit(u8 bit)
{
    DEV_STATUS_STRUCT status;
    cmd_get_dsp_status(&status);
    status.ports |= bit;
    cmd_set_dsp_status(&status);
}


#pragma section("FLASH_code")
static void clr_status_bit(u8 bit)
{
    DEV_STATUS_STRUCT status;
    cmd_get_dsp_status(&status);
    status.ports &= ~bit;
    cmd_set_dsp_status(&status);
}



/**
 * ����������� �� �������� ������ PD0 ����������
 */
#pragma section("FLASH_code")
void select_gps_module(void)
{
    pin_set(GPS_EN_PORT, GPS_EN_PIN);
    set_status_bit(RELE_GPS_BIT);
}

/**
 * ��������� ��������
 */
#pragma section("FLASH_code")
void unselect_gps_module(void)
{
    pin_clr(GPS_EN_PORT, GPS_EN_PIN);
    clr_status_bit(RELE_GPS_BIT);
}


/**
 * �������� ���� ������
 */
#pragma section("FLASH_code")
void modem_on(void)
{
    RELE_on(RELEAM);
    set_status_bit(RELE_MODEM_BIT);
}


/**
 * ��������� �����
 */
#pragma section("FLASH_code")
void modem_off(void)
{
    RELE_off(RELEAM);
    clr_status_bit(RELE_MODEM_BIT);
}


/**
 * �������� �������
 */
#pragma section("FLASH_code")
void burn_wire_on(void)
{
    pin_set(GATEBURN_PORT, GATEBURN_PIN);
    set_status_bit(RELE_BURN_BIT);
}

/**
 * ��������� �������
 */
#pragma section("FLASH_code")
void burn_wire_off(void)
{
    pin_clr(GATEBURN_PORT, GATEBURN_PIN);
    clr_status_bit(RELE_BURN_BIT);
}


/**
 * �������� ���������� �����
 */
#pragma section("FLASH_code")
void select_analog_power(void)
{
#if QUARTZ_CLK_FREQ==(8192000)
    TIMER3_enable();
#endif
    pin_set(APWR_EN_PORT, APWR_EN_PIN);
    set_status_bit(RELE_ANALOG_POWER_BIT);
}

/**
 * ��������� ���������� �����
 */
#pragma section("FLASH_code")
void unselect_analog_power(void)
{
    pin_clr(APWR_EN_PORT, APWR_EN_PIN);
    clr_status_bit(RELE_ANALOG_POWER_BIT);
    TIMER3_disable();
}


/**
 * ����������� �� UART - 0 ��� UART
 */
#pragma section("FLASH_code")
void select_debug_module(void)
{
    pin_clr(UART_SELA_PORT, UART_SELA_PIN);
    set_status_bit(RELE_DEBUG_MODULE_BIT);
    clr_status_bit(RELE_MODEM_MODULE_BIT);
}


/**
 * ����������� �� ���������� �����
 */
#pragma section("FLASH_code")
void select_modem_module(void)
{
    pin_set(UART_SELA_PORT, UART_SELA_PIN);
    set_status_bit(RELE_MODEM_MODULE_BIT);
    clr_status_bit(RELE_DEBUG_MODULE_BIT);
}

/**
 * ��������� UART
 */
#pragma section("FLASH_code")
void unselect_debug_uart(void)
{
    pin_hiz(USB_EN_PORT, USB_EN_PIN);	/* �������� ����������� */
    pin_clr(FT232_RST_PORT, FT232_RST_PIN);	/* ��������� ft232  */
    clr_status_bit(RELE_MODEM_MODULE_BIT);
    clr_status_bit(RELE_DEBUG_MODULE_BIT);
}




/**
 * ���������� ������������ USB, ���������� ����� ��� ���
 * ����� - ���� WUSB, 0 - ���������� ����� ��������
 */
#pragma section("FLASH_code")
bool wusb_on(void)
{
    u8 pin;
    bool ret;


    pin_clr(USBEXT33_EN_PORT, USBEXT33_EN_PIN);
    pin_clr(USB_EN_PORT, USB_EN_PIN);	/* ��������� ������� USB */


    pin_clr(HUB_RST_PORT, HUB_RST_PIN);	/* ����� ����  */
    delay_ms(5);			/* �������� */

    pin_set(HUB_RST_PORT, HUB_RST_PIN);	/* ����� ����  */
    delay_ms(250);			/* �������� */
    /* ���������, ���� �� ������� ������� � USB(���� 0), ���� ���� 1 - WUSB */
    pin = pin_get(USB_VBUSDET_BUF_INPUT_PORT);
    ret = ((pin & (1 << USB_VBUSDET_BUF_PIN)) ? true : false);


    // ������ ��������
    if ((pin & (1 << USB_VBUSDET_BUF_PIN)) == 0) {
	pin_clr(WUSB_EN_PORT, WUSB_EN_PIN);	/* ������� ������� � Alereon - ��� WUSB */
        set_status_bit(RELE_USB_BIT);
        clr_status_bit(RELE_WUSB_BIT);
    } else {
	pin_set(WUSB_EN_PORT, WUSB_EN_PIN);	/* ������ ������� �� Alereon - ���� WUSB */
	clr_status_bit(RELE_USB_BIT);
        set_status_bit(RELE_WUSB_BIT);
    }

    pin_clr(FT232_RST_PORT, FT232_RST_PIN);	/* Reset  */
    delay_ms(250);
    pin_set(FT232_RST_PORT, FT232_RST_PIN);	/* ������� reset  */
    delay_ms(125);

    return ret;
}

/**
 * ��������� ������������ USB, � ��� ��������� USB
 */
#pragma section("FLASH_code")
void wusb_off(void)
{
    pin_clr(WUSB_EN_PORT, WUSB_EN_PIN);	/* ��������� WUSB_EN_PIN � "0" */
    pin_set(USB_EN_PORT, USB_EN_PIN);	/* ��������� USB */
    pin_clr(FT232_RST_PORT, FT232_RST_PIN);	/* ��������� ft232  */
    pin_set(UART_SELA_PORT, UART_SELA_PIN);	/* ��������� UART. ��������� ��� ������ */

    pin_clr(AT_SD_CD_PORT, AT_SD_CD_PIN);	/* ������ � Z */
    pin_clr(AT_SD_WP_PORT, AT_SD_WP_PIN);	/* ������ � Z */
    pin_clr(HUB_RST_PORT, HUB_RST_PIN);	/* ������ � 0 */
    clr_status_bit(RELE_USB_BIT);
    clr_status_bit(RELE_WUSB_BIT);
}


/**
 * SD ����� ��������� � ����?
 */
#pragma section("FLASH_code")
bool check_sd_card(void)
{
    u8 res;

    /* ���� ������ */
    res = (*pPORTGIO & PG1);

    if (res)
	pin_clr(AT_SD_CD_PORT, AT_SD_CD_PIN);
    else
	pin_set(AT_SD_CD_PORT, AT_SD_CD_PIN);

    return res;
}

/**
 * �������� SD ����� 
 * SD ����� ���������� � BF
 */
#pragma section("FLASH_code")
void select_sdcard_to_bf(void)
{
    pin_clr(SD_SRCSEL_PORT, SD_SRCSEL_PIN);
    delay_ms(100);
    pin_clr(SD_EN_PORT, SD_EN_PIN);
    delay_ms(10);
    pin_set(AT_SD_WP_PORT, AT_SD_WP_PIN);
    delay_ms(100);
    pin_clr(AT_SD_WP_PORT, AT_SD_WP_PIN);
    delay_ms(10);
    pin_set(AT_SD_CD_PORT, AT_SD_CD_PIN);
    delay_ms(50);
}


/**
 * �������� SD ����� � ����������. SD ����� ���������� � ����������
 */
#pragma section("FLASH_code")
void select_sdcard_to_cr(void)
{
    pin_clr(USB_EN_PORT, USB_EN_PIN);	/* 0 - �������� */
    pin_set(HUB_RST_PORT, HUB_RST_PIN);	/* ������� reset � HUB */
    delay_ms(5);
    pin_set(SD_SRCSEL_PORT, SD_SRCSEL_PIN);
    pin_clr(SD_EN_PORT, SD_EN_PIN);
    pin_clr(AT_SD_WP_PORT, AT_SD_WP_PIN);
    pin_set(AT_SD_CD_PORT, AT_SD_CD_PIN);
    delay_ms(5);
    pin_clr(AT_SD_CD_PORT, AT_SD_CD_PIN);
}


/**
 * ����� ������� ������
 */
#pragma section("FLASH_code")
void old_modem_reset(void)
{
    pin_clr(AM_RST_PORT, AM_RST_PIN);
    delay_ms(250);
    LED_toggle(LED_GREEN);

    pin_set(AM_RST_PORT, AM_RST_PIN);
    delay_ms(250);
    LED_toggle(LED_GREEN);

    pin_clr(AM_RST_PORT, AM_RST_PIN);
    delay_ms(250);
    LED_toggle(LED_GREEN);
}
