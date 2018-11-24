/*******************************************************************
 * UART0 ����� ������ NMEA �� GPS, ���������� ��� ������
 * ������ ������ ���������!!!
 * ��������� ������ �� ������ $GPRMC!!!
*******************************************************************/
#include <string.h>
#include "uart0.h"
#include "utils.h"
#include "irq.h"
#include "log.h"
#include "pll.h"

/************************************************************************
 * 	����������� ����������
 ************************************************************************/
static DEV_UART_STRUCT uart0_xchg_struct;



/**
 * Function:    UART_init - ����������� ����� �� 
 * ����� �������� EDBO
 */
#pragma section("FLASH_code")
bool UART0_init(void *dev)
{
    volatile int temp, i;
    DEV_UART_STRUCT *comm;
    u16 divider;
    bool res = false;

    do {
	if (dev == NULL)
	    break;

	comm = (DEV_UART_STRUCT *) dev;
	if (comm->baud <= 0) {
	    log_write_log_file("ERROR: UART0 baud can't be zero\n");
	    break;
	}

	*pPORTG_FER |= (PG12 | PG13);	/* ��������� ������� �� ������ Rx � Tx - ��� ��������� �� PG12 � PG13 */
	*pPORTG_MUX &= ~(PG12 | PG13);	/* �������� 3-� ������� 00 �� 12:13 */
	ssync();

	/* ���� UART clock + ��������-��������� = 1 + rerout �� ������� ���������� - ��� EDBO */
	*pUART0_GCTL = EGLSI | UCEN | EDBO;
	ssync();


	divider = (SCLK_VALUE / comm->baud);
	*pUART0_DLL = divider;
	*pUART0_DLH = (divider >> 8);


	*pUART0_LCR = WLS_8;	/* 8N1 */
	temp = *pUART0_RBR;
	temp = *pUART0_LSR;

	*pUART0_IER_CLEAR = 0xff;
	*pUART0_IER_SET = ERBFI | ELSI;	/* �������� ����� ����� + ������ */
	ssync();

	uart0_xchg_struct.rx_call_back_func = (void (*)(u8)) comm->rx_call_back_func;	/* ��������� �� ������� ������ */
	uart0_xchg_struct.tx_call_back_func = (void (*)(void)) comm->tx_call_back_func;	/* ��������� �� ������� ������ */
	IRQ_register_vector(NMEA_VECTOR_NUM);	/* ������������ ���������� UART0 */

	*pSIC_IMASK0 |= IRQ_UART0_ERR;	/* ������ �� ������� ��� UART0 */
	*pSIC_IAR0 &= 0xFF0FFFFF;	/* 5-� ��������  (� ����) */
	*pSIC_IAR0 |= 0x00200000;	/* STATUS IRQ: IVG09 */
	ssync();

	log_write_log_file("INFO: UART0 init on %d baud OK\n", comm->baud);
	uart0_xchg_struct.is_open = true;
	res = true;
    } while (0);
    return res;
}

/**
 * ��������� UART0, ��������� ������ 
 */
#pragma section("FLASH_code")
void UART0_close(void)
{
    *pUART0_GCTL = 0;
    ssync();
    uart0_xchg_struct.is_open = false;
    log_write_log_file("INFO: UART0 closed OK\n");
}

/**
 * ������� ������ ����� UART0
 * ����� ����� �������. ����� ���������� ��� � ������� ��� � ������ 
 */
#pragma section("FLASH_code")
int UART0_write_str(char *str, int len)
{
    int i = -1;

    /* ���� ������ */
    if (uart0_xchg_struct.is_open) {
////	IRQ_unregister_vector(NMEA_VECTOR_NUM);	
	for (i = 0; i < len; i++) {
	    while (!(*pUART0_LSR & THRE));
	    *pUART0_THR = str[i];
	    ssync();
	}
////	IRQ_register_vector(NMEA_VECTOR_NUM);	/* ������������ ���������� UART0 */
    }
    return i;
}

/**
 * ������� ���������� - �������� ����� ����� - ��������� ������ �� $GPRMC!
 */
#pragma section("FLASH_code")
//#pragma section("L1_code")
void UART0_STATUS_ISR(void)
{
    volatile c8 byte;

    register volatile u16 stat = *pUART0_LSR;	/* �������... */
    ssync();

    /* ������� �������� ������ - ���� 1..4 */
    if (stat & 0x1E) {
	*pUART0_LSR = stat & 0x1E;
	byte = *pUART0_RBR;	// ������ �� ������
	ssync();
    }


    if (stat & DR) {
	byte = *pUART0_RBR;	/* �������� ���� */
	ssync();

	/* �������� ������� callback */
	if (uart0_xchg_struct.rx_call_back_func != NULL) {
	    uart0_xchg_struct.rx_call_back_func(byte);
	}

    } else if (stat & THRE) {	/*  �������� ����� */
	if (uart0_xchg_struct.tx_call_back_func != NULL) {
	    uart0_xchg_struct.tx_call_back_func();
	}
    }
}
