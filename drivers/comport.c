#include <string.h>
#include <stdio.h>
#include "comport.h"
#include "com_cmd.h"
#include "main.h"
#include "crc16.h"
#include "log.h"
#include "led.h"
#include "uart1.h"
#include "gps.h"
#include "led.h"
#include "utils.h"
#include "ports.h"


//#define 	UART_DEBUG_SPEED		115200
//#define 	UART_DEBUG_SPEED		230400	/* �������� ����� ��� ������ � PC  */
#define 	UART_DEBUG_SPEED		460800
#define 	RX_BUF_LEN			255
#define 	TX_BUF_LEN			255


/************************************************************************
 * 	����������� ����������
 ************************************************************************/
/* ��������� �� ���� ��������� - ����� �� ����� */
static struct RX_DATA_STRUCT {
    u8 rx_buf[RX_BUF_LEN];	/* �� ����� */
    u8 tx_buf[TX_BUF_LEN];	/* � �� �������� */

    u8 rx_beg;			/* ������ ������ */
    u8 rx_cmd;			/* �������  */
    u8 rx_cnt;			/* ������� ��������� */
    u8 rx_len;			/* ����� ������� */

    u8 rx_fin;			/* ����� ������ */
    u8 rx_ind;
    u8 tx_cnt;
    u8 tx_len;

    u8 tx_ind;
    u8 tx_fin;			/* ����� ������ */
    u16 crc16;			/* ����������� ����� CRC16 */
} *xchg_buf;

/************************************************************************
 * 	����������� �������
 ************************************************************************/
static void comport_debug_write_ISR(void);
static void comport_debug_read_ISR(u8);
static void comport_rx_end(int);


/**
 * Description: �������������� UART �� ����� ����� �������
 */
#pragma section("FLASH_code")
int comport_init(void)
{
    DEV_UART_STRUCT com_par;
    int res = -1;

#if 1
    select_debug_module();	/* ����������� �������� �� ���������� ����  */
#else

    select_modem_module(); /* ���� ��� ������ */
#endif

    /* ��� ������������ ������ ������� ����� �� ����� */
    if (xchg_buf == NULL) {
	xchg_buf = calloc(1, sizeof(struct RX_DATA_STRUCT));
	if (xchg_buf == NULL) {
	    log_write_log_file("Error: can't alloc buf for comport\n");
	    return -2;
	}
    } else {
	log_write_log_file("WARN: comport buf dev already exists\n");
    }

    /* �������� UART1 init */
    com_par.baud = UART_DEBUG_SPEED;
    com_par.rx_call_back_func = comport_debug_read_ISR;
    com_par.tx_call_back_func = comport_debug_write_ISR;

    if (UART1_init(&com_par) == true) {
	res = 0;
    }

    xchg_buf->crc16 = 0xAA;
    return res;			// ��� ��
}

/* ������� UART  */
#pragma section("FLASH_code")
void comport_close(void)
{
    UART1_close();

    if (xchg_buf) {
	free(xchg_buf);		/* ����������� �����  */
	xchg_buf = NULL;
    }
}



/* ������ ���������� ������� ��� 0 */
#pragma section("FLASH_code")
u8 comport_get_command(void)
{
    if (xchg_buf->rx_fin)
	return xchg_buf->rx_cmd;
    else
	return 0;
}



/* ������������ ���������� ������ */
section("L1_code")
static void comport_debug_read_ISR(u8 rx_byte)
{
    u8 cPar;
    u16 wPar;
    u32 lPar0, lPar1, lPar2;
    DEV_UART_COUNTS cnt;
    DEV_UART_CMD uart_cmd;	/* ������� ������ � UART */

    /* ��������� CRC16. ��� ���������� ������ ��������� ����� �.�. = 0 */
    xchg_buf->rx_ind = (u8) ((xchg_buf->crc16 >> 8) & 0xff);
    xchg_buf->crc16 = (u16) (((xchg_buf->crc16 & 0xff) << 8) | (rx_byte & 0xff));
    xchg_buf->crc16 ^= get_crc16_table(xchg_buf->rx_ind);


    /* ������ ����� ������ ���� ������� � ��� ����� */
    if (xchg_buf->rx_beg == 0 && rx_byte == 0xFF) {	/* ������ ���� �������: ����� - FF */
	xchg_buf->rx_beg = 1;	/* ���� ���� ������� */
	xchg_buf->crc16 = rx_byte;	/* ����������� ����� ����� ������� ����� */
	xchg_buf->rx_cnt = 1;	/* ������� ������� - ������ ���� ������� */
    } else {			/* ������ ����������� ����� */
	if (xchg_buf->rx_cnt == 1) {
	    if (rx_byte != 0)	// ���� ��������
		comport_rx_end(3);
	} else if (xchg_buf->rx_cnt == 2) {
	    if (rx_byte == 0)	// ���� ��������-���� ���� �� �����
		comport_rx_end(3);
	    xchg_buf->rx_len = rx_byte;	/* ������ ���� ��� ����� ���� ��������� ������� - �� ����� ���� 0! */

	    /* 3...��� ��������� ��������� ��� ������ ������� ���� ��� ����. ��� ��� ������� ������ � ��. */
	} else if (xchg_buf->rx_cnt > 2 && xchg_buf->rx_cnt < (xchg_buf->rx_len + 3)) {	/* ������ ���������� � 4-�� ����� */
	    xchg_buf->rx_buf[xchg_buf->rx_cnt - 3] = rx_byte;	/* � �������� ����� �������� ������� */

	} else if (xchg_buf->rx_cnt == (xchg_buf->rx_len + 3)) {	/* ��. ���� ����������� �����  */
	    asm("nop;");	/* ������ �� ������ - ����� ����� �� ������� � ����. ������� */
	} else if (xchg_buf->rx_cnt == (xchg_buf->rx_len + 4)) {	/* ��. ���� ����������� �����  */

	    /*  Crc16 ���������� ? */
	    if (xchg_buf->crc16 == 0) {
		comport_rx_end(0);

		// ����� ���-�������?
		xchg_buf->rx_cmd = xchg_buf->rx_buf[0];

		/* ������ ������  */
		switch (xchg_buf->rx_cmd) {

/************************************************************************************************
 * ����������� ������� 
 ************************************************************************************************/
		    /* �������� ���� ��� - ����� � ������ �������� */
		case UART_CMD_COMMAND_PC:
		    cmd_get_dsp_addr(xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

		    /* ������ ������ ����������� - ����� � ������ �������� */
		case UART_CMD_GET_DSP_STATUS:
		    cmd_get_dsp_status(xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

		    /* �������� ����� GNS110 - ����� � ������ �������� */
		case UART_CMD_GET_RTC_CLOCK:
		    cmd_get_gns110_rtc(xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

		    /* ������ �������� ������� - ����� ����� ��������� (��� ��� CRC � ��������� len, ������� -3 �� ������  */
		case UART_CMD_GET_COUNTS:
		    UART1_get_count(&cnt);	/* �������� �������� ������ */
		    xchg_buf->tx_buf[0] = sizeof(DEV_UART_COUNTS);
		    memcpy(&xchg_buf->tx_buf[1], &cnt, sizeof(DEV_UART_COUNTS));
		    UART1_start_tx();
		    break;


		    /* �������� ����� ������ */
		case UART_CMD_CLR_BUF:
		    cmd_clear_adc_buf(xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

		    /* ������ ������������ */
		case UART_CMD_INIT_TEST:
		    uart_cmd.cmd = UART_CMD_INIT_TEST;	// �������
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

		    /* �������� ��� ������� ������ DSP */
		case UART_CMD_GET_WORK_TIME:
		    cmd_get_work_time(xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

		    /* �������� EEPROM */
		case UART_CMD_ZERO_ALL_EEPROM:
		    uart_cmd.cmd = UART_CMD_ZERO_ALL_EEPROM;	// �������
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();	/* �������� */
		    break;

		    /* ���������� reset DSP */
		case UART_CMD_DSP_RESET:
		    uart_cmd.cmd = UART_CMD_DSP_RESET;	// �������
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

		    /* ���������� ������� DSP: poweroff */
		case UART_CMD_POWER_OFF:
		    uart_cmd.cmd = UART_CMD_POWER_OFF;	// �������
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

		    /* ������� �������� ���� ������ */
		case UART_CMD_MODEM_ON:
		    uart_cmd.cmd = UART_CMD_MODEM_ON;
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();	/* �������� */
		    break;

		    /* ������� ��������� ���� ������ */
		case UART_CMD_MODEM_OFF:
		    uart_cmd.cmd = UART_CMD_MODEM_OFF;
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();	/* �������� */
		    break;

		    /* ������� �������� */
		case UART_CMD_BURN_ON:
		    uart_cmd.cmd = UART_CMD_BURN_ON;
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();	/* �������� */
		    break;

		    /* ������� ��������� */
		case UART_CMD_BURN_OFF:
		    uart_cmd.cmd = UART_CMD_BURN_OFF;
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();	/* �������� */
		    break;

		    /* ������� �������� GPS */
		case UART_CMD_GPS_ON:
		    uart_cmd.cmd = UART_CMD_GPS_ON;
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();	/* �������� */
		    break;

		    /* ������� ��������� GPS */
		case UART_CMD_GPS_OFF:
		    uart_cmd.cmd = UART_CMD_GPS_OFF;
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();	/* �������� */
		    break;

		    /* ������� ������ ������ NMEA */
		case UART_CMD_NMEA_GET:
		    if (gps_get_nmea_string((c8 *) xchg_buf->tx_buf + 1, NMEA_GPRMC_STRING_SIZE) > 0) {
			xchg_buf->tx_buf[0] = NMEA_GPRMC_STRING_SIZE;
		    } else {
			cmd_get_dsp_status(xchg_buf->tx_buf);
			xchg_buf->tx_buf[0] = 2;	// 2 ����� - �� ������ ���!
		    }
		    UART1_start_tx();	/* �������� */
		    break;


		    /*  ������ ������ */
		case UART_CMD_GET_DATA:
		    cmd_get_adc_data(xchg_buf->tx_buf);
		    UART1_start_tx();	/* �������� */
		    break;


		    /* �������� ���� ��� � �����. �������� u16 � �������� ������ � ������ + 1 */
		case UART_CMD_SET_DSP_ADDR:
		    wPar = get_short_from_buf(xchg_buf->rx_buf, 1);
		    cmd_set_dsp_addr(wPar, xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

/***********************************************************************************************
 * 	����������� �������  
 ************************************************************************************************/
		    /* ������������� RTC, �������� � �������� ������ + 1 ���� */
		case UART_CMD_SYNC_RTC_CLOCK:
		    lPar0 = get_long_from_buf(xchg_buf->rx_buf, 1);
		    cmd_set_gns110_rtc(lPar0, xchg_buf->tx_buf);
		    UART1_start_tx();	/* �������� */
		    break;

/***********************************************************************************************
 * 	12 - ������� �������  
 ************************************************************************************************/
		    /* ���������� / �������� ��� ������� ������ DSP */
		case UART_CMD_SET_WORK_TIME:
		    lPar0 = get_long_from_buf(xchg_buf->rx_buf, 1);
		    lPar1 = get_long_from_buf(xchg_buf->rx_buf, 5);
		    lPar2 = get_long_from_buf(xchg_buf->rx_buf, 9);
		    cmd_set_work_time(lPar0, lPar1, lPar2, xchg_buf->tx_buf);
		    UART1_start_tx();	/* �������� */
		    break;

/***********************************************************************************************
 * 	������������ �������  
 ************************************************************************************************/
		    /* ���������� / �������� � EEPROM ��������� */
		case UART_CMD_SET_ADC_CONST:
		    cmd_set_adc_const(&xchg_buf->rx_buf[1], xchg_buf->tx_buf);	/* �������� ����� � ���������. ������ ���� ����� ������� - �� ���������! */
		    UART1_start_tx();	/* �������� */
		    break;

		    /* �������� ��������� ���� �������. ������ ���� ����� ����� �������  */
		case UART_CMD_GET_ADC_CONST:
		    cmd_get_adc_const(xchg_buf->tx_buf);
		    UART1_start_tx();	/* �������� */
		    break;


		    /* ����� ���������. ������ ���� ����� ������� - �� ���������! */
		case UART_CMD_DEV_START:
		    uart_cmd.cmd = UART_CMD_DEV_START;	/* ������� ���� ������ ��� ���� �������? 24 ����� */
		    memcpy(uart_cmd.u.cPar, &xchg_buf->rx_buf[1], 24);
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();	/* �������� */
		    break;

		    /* ������� c��� ��������� */
		case UART_CMD_DEV_STOP:
		    uart_cmd.cmd = UART_CMD_DEV_STOP;	/* ����, ���������� ��� */
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;


/***********************************************************************************************
 * 	������������ ������� - ������� ������
 ************************************************************************************************/

		    /* ��������������� ������� ������ - � ������ ������ � 2 �� ���� ��������� ������� ������. 
		     * � ��������� ������, CRC16.
		     * ����� ������� ���� ������� - ���� ������ ����������� �� ���. ����� */
		case UART_CMD_MODEM_REQUEST:
		    uart_cmd.cmd = UART_CMD_MODEM_REQUEST;	// ������� ���������
		    cPar = xchg_buf->rx_buf[0] - 1;	// ����� ������ ��� �������
		    uart_cmd.len = cPar % MODEM_BUF_LEN;	// ������� � ��� ����� �����
		    memcpy(&uart_cmd.u.cPar, xchg_buf->rx_buf + 1, cPar % MODEM_BUF_LEN);	// ����������� ��� main.c
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);	// � tx_buf ����� �����
		    UART1_start_tx();	/* �������� */
		    break;

		    /* �������� ����� ������ - �� ������ ������ */
		case UART_CMD_GET_MODEM_REPLY:
		    get_uart_cmd_buf(&uart_cmd);	// �����
		    cPar = uart_cmd.len % MODEM_BUF_LEN;	// ����� ��������� (�� ����� 64)
		    if (cPar > 0) {
			memcpy(&xchg_buf->tx_buf[1], uart_cmd.u.cPar, cPar);	// �������� � ����� UART
			xchg_buf->tx_buf[0] = cPar;	// ����� ���������
			memset(&uart_cmd, 0, sizeof(uart_cmd));
		    } else {
			cmd_get_dsp_status(xchg_buf->tx_buf);
			xchg_buf->tx_buf[0] = 2;
			xchg_buf->tx_buf[1] |= 0x08;	// ������ �� ������ ���
		    }
		    UART1_start_tx();	/* �������� */
		    break;


		default:
		    comport_rx_end(2);
		    break;
		}

	    } else {		/* ����������� ����� �������-�������� ����� ������ */
		comport_rx_end(1);
	    }

	} else {		/* ���� ���-�� ����� �� ��� */
	    comport_rx_end(2);
	}
	xchg_buf->rx_cnt++;	/* ������� ������� */
    }
}


/**
 * ����� ������
 */
section("L1_code")
static void comport_rx_end(int err)
{
    xchg_buf->rx_beg = 0;
    xchg_buf->rx_cnt = 0;

    if (err == 0) {
	xchg_buf->rx_fin = 1;	/* ������� ������� OK */
	xchg_buf->tx_cnt = 0;	/* ����� ���������� */
	xchg_buf->tx_fin = 0;	/* ������ �������� */
    } else if (err == 1) {	/* ����������� ����� �������-�������� ����� ������ */
	xchg_buf->rx_fin = 0;	/* ������� �������� Error */
    } else {			/* ���� ���-�� ����� �� ��� */
    }
}


/* ������������ ���������� ������ (������) */
section("L1_code")
static void comport_debug_write_ISR(void)
{
    register u8 tx_byte;

    if (xchg_buf->tx_cnt == 0) {
	xchg_buf->tx_len = xchg_buf->tx_buf[0];	// ����� �������� �������

	if (xchg_buf->tx_len == 0)
	    return;

	tx_byte = xchg_buf->tx_len;
	xchg_buf->crc16 = tx_byte;	/* ������ ���� � crc - ����� */
	xchg_buf->tx_ind = 0;
    } else if (xchg_buf->tx_cnt <= xchg_buf->tx_len) {
	tx_byte = xchg_buf->tx_buf[xchg_buf->tx_cnt];
	xchg_buf->tx_ind = (u8) ((xchg_buf->crc16 >> 8) & 0xff);
	xchg_buf->crc16 = (u16) (((xchg_buf->crc16 & 0xff) << 8) | (tx_byte & 0xff));
	xchg_buf->crc16 ^= get_crc16_table(xchg_buf->tx_ind);
    } else if (xchg_buf->tx_cnt == xchg_buf->tx_len + 1) {	/* ������� ����������� ����� �������� 2 ����! */
	xchg_buf->tx_ind = (u8) ((xchg_buf->crc16 >> 8) & 0xff);
	xchg_buf->crc16 = (u16) ((xchg_buf->crc16 & 0xff) << 8);
	xchg_buf->crc16 ^= get_crc16_table(xchg_buf->tx_ind);

	xchg_buf->tx_ind = (u8) ((xchg_buf->crc16 >> 8) & 0xff);
	xchg_buf->crc16 = (u16) ((xchg_buf->crc16 & 0xff) << 8);
	xchg_buf->crc16 ^= get_crc16_table(xchg_buf->tx_ind);

	tx_byte = (xchg_buf->crc16 >> 8) & 0xff;	/* ��. ���� CRC16 */
    } else if (xchg_buf->tx_cnt == xchg_buf->tx_len + 2) {
	tx_byte = xchg_buf->crc16 & 0xff;	/* ��. ���� CRC16 */
	xchg_buf->tx_fin = 1;
	UART1_stop_tx();	/* ��������� ���������� */
    } else {
	UART1_stop_tx();	/* ��������� ����������. �� ������... */
    }
    UART1_tx_byte(tx_byte);
    xchg_buf->tx_cnt++;
}
