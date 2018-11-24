/*******************************************************************
 * GPS ����� ������ NMEA
 * ��������� ������ �� ������ $GPRMC!!!
*******************************************************************/
#include <string.h>
#include <stdio.h>

#include "uart0.h"
#include "timer4.h"
#include "xpander.h"
#include "timer4.h"
#include "utils.h"
#include "ports.h"
#include "gps.h"
#include "log.h"
#include "led.h"
#include "pll.h"

#define 	GPS_NMEA_OLD_SPEED  		UART_SPEED_115200	/* �������� �������� */
#define 	GPS_NMEA_NEW_SPEED  		UART_SPEED_9600	/* �������� �������� */
#define 	RX_FIN_SIGN 			0x5A

/* ����� �� UART0 - �������� ������ �� ��������� �� ���� ��������� */
static struct NMEA_DATA_STRUCT {
    u32 rx_nmea_time;		/* ����� �� ������ - ����������� ��� ������� 4 */
    s64 dif_time;

    s32 lat;			/* ������ */
    s32 lon;			/* �������  */

    c8 rx_pmtk_buf[NMEA_PMTK_STRING_SIZE];	/* ���� ������� ��� ������� PMTK, � ������ ���� - ������� �� rx_buf_out */
    c8 rx_buf_in[NMEA_GPRMC_STRING_SIZE];	/* ���� ������� �������� */
    c8 rx_buf_out[NMEA_GPRMC_STRING_SIZE];	/* ���� ������� ��� ������� */

    u8 rx_beg;
    u8 rx_num;
    u8 rx_cnt;
    bool rx_check;		/* �������������  */

    u8 rx_pmtk_len;
    u8 rx_exist;		/* NMEA ���� ����� */
    u8 rx_fin;
} *gps_data_ptr;


static long gps_parse_nmea_string(void);
static void gps_nmea_read_ISR(u8);	/* ������ �� UART0 */

/**
 * Function:    gps_init - ����������� ����� �� 
 * ����� �������� EDBO
 */
#pragma section("FLASH_code")
int gps_init(void)
{
    DEV_UART_STRUCT gps_par;
    int res = -1;
    int t0, t1;

    select_gps_module();	/* ��� GPS  - ��������� UART0 */

    /* ������� ������� ����� �� ����� */
    if (gps_data_ptr == NULL) {
	gps_data_ptr = calloc(sizeof(struct NMEA_DATA_STRUCT), 1);
	if (gps_data_ptr == NULL) {
	    log_write_log_file("Error: can't alloc buf for NMEA\n");
	    return -1;
	}
    } else {
	log_write_log_file("WARN: buf fo NMEA already exist!\n");
	return 0;
    }


    /* �������� UART0 init �� 115200 */
    gps_par.baud = GPS_NMEA_OLD_SPEED;
    gps_par.rx_call_back_func = gps_nmea_read_ISR;
    gps_par.tx_call_back_func = NULL;	/* ���  */

    if (UART0_init(&gps_par) == true) {


#if 1
	/* ������� "�����������"  */
	t0 = get_msec_ticks();
	t1 = t0;
	while (t1 - t0 < 250) {
	    LED_blink();
	    t1 = get_msec_ticks();
	}
	gps_wake_up();
#endif


	/* �������� �������� ����� */
	t0 = get_msec_ticks();
	t1 = t0;
	while (t1 - t0 < 250) {
	    LED_blink();
	    t1 = get_msec_ticks();
	}
	res = gps_change_baud();

#if 0
	/* ������� ��� ������� */
	t0 = get_msec_ticks();
	t1 = t0;
	while (t1 - t0 < 250) {
	    LED_blink();
	    t1 = get_msec_ticks();
	}
       gps_set_zda();
#endif
    }

    return res;
}


/**
 * ��������� UART0, ��������� ������ 
 */
#pragma section("FLASH_code")
void gps_close(void)
{
    gps_standby();
    UART0_close();
    unselect_gps_module();

    if (gps_data_ptr) {
	free(gps_data_ptr);
	gps_data_ptr = NULL;
    }

}


/**
 * ������� ���������� - �������� ����� ����� - ��������� ������ �� $GPRMC!
 */
#pragma section("FLASH_code")
static void gps_nmea_read_ISR(u8 rx_byte)
{				/* ������ �� UART0 */
    char buf[32];

    /*  ������ ���� */
    if (rx_byte == '$') {	/* ���������� ������ � $ */
	gps_data_ptr->rx_beg = 1;
	gps_data_ptr->rx_cnt = 1;
	gps_data_ptr->rx_buf_in[0] = '$';

	//   gps_data_ptr->rx_nmea_time = 0;      // ��������� �������� �����
	gps_data_ptr->rx_fin = 0;	/* ���� ��������� ������ ������  ��� ������ GPRMS!!!! */
	gps_data_ptr->rx_exist = 0;
    } else if (gps_data_ptr->rx_beg == 1) {	// ����� ��� �������
	gps_data_ptr->rx_buf_in[gps_data_ptr->rx_cnt] = rx_byte;

	/*  ��������, ������� ������/ �������� ������ RMC � GSA � PMTK */
	if (rx_byte == '\x0A') {

	    // ����� �� ������� PMTK
	    if (gps_data_ptr->rx_buf_in[1] == 'P' && gps_data_ptr->rx_buf_in[2] == 'M' && gps_data_ptr->rx_buf_in[3] == 'T'
		&& gps_data_ptr->rx_buf_in[4] == 'K') {
		memcpy(gps_data_ptr->rx_pmtk_buf, gps_data_ptr->rx_buf_in, gps_data_ptr->rx_cnt % (NMEA_PMTK_STRING_SIZE - 1));	// ����������� � �����
		gps_data_ptr->rx_pmtk_buf[gps_data_ptr->rx_cnt - 1] = 0;
		gps_data_ptr->rx_pmtk_len = gps_data_ptr->rx_cnt;
		return;
	    }
	    // GPRMC - ���� ����� ����� PC!
	    // $GPRMC,085827.649,A,5541.7900,N,03721.3821,E,0.06,0.00,181113,10.0,E,A*36
	    if (gps_data_ptr->rx_buf_in[1] == 'G' && gps_data_ptr->rx_buf_in[3] == 'R' && gps_data_ptr->rx_buf_in[4] == 'M'
		&& gps_data_ptr->rx_buf_in[5] == 'C') {
		memcpy(gps_data_ptr->rx_buf_out, gps_data_ptr->rx_buf_in, gps_data_ptr->rx_cnt);
		gps_data_ptr->rx_buf_out[gps_data_ptr->rx_cnt - 1] = 0;
		gps_data_ptr->rx_beg = 0;
		gps_data_ptr->rx_cnt = 0;
		gps_data_ptr->rx_nmea_time = gps_parse_nmea_string();
		gps_data_ptr->rx_fin = RX_FIN_SIGN;	/* ���� ��������� ������ ������  ��� ������ GPRMC!!!! */
		return;
	    }
	    // �����  $GPGSA,A,3,17,28,11,04,,,,,,,,,3.20,3.09,1.51*04
	    if (gps_data_ptr->rx_buf_in[1] == 'G' && gps_data_ptr->rx_buf_in[3] == 'G' && gps_data_ptr->rx_buf_in[4] == 'S'
		&& gps_data_ptr->rx_buf_in[5] == 'A') {
		gps_data_ptr->rx_check = ((gps_data_ptr->rx_buf_in[9] == '3') ? true : false);	/* ���� 3dFix? */
		gps_data_ptr->rx_buf_in[1] = 0;	// ������ memset
		gps_data_ptr->rx_beg = 0;
		gps_data_ptr->rx_cnt = 0;
		gps_data_ptr->rx_exist = 1;
	    }

	} else {
	    gps_data_ptr->rx_cnt++;
	    if (gps_data_ptr->rx_cnt >= 120) {
		gps_data_ptr->rx_cnt = 0;
		gps_data_ptr->rx_beg = 0;
	    }
	}
    }
}


/**
 * �������� ������ �� NMEA
 */
#pragma section("FLASH_code")
int gps_get_nmea_string(char *str, int len)
{
    int res = 0;

    if (gps_data_ptr->rx_fin == RX_FIN_SIGN) {
	strncpy(str, (char *) gps_data_ptr->rx_buf_out, len);
	gps_data_ptr->rx_fin = 0;
	res = len;
    }
    return res;
}


/**
 * �������� ������ �� NMEA
 */
#pragma section("FLASH_code")
int gps_get_pmtk_string(char *str, int n)
{
    int res = 0;

    /* ���� ���� PMTK - ������� ������ �� */
    if (gps_data_ptr->rx_pmtk_len) {
	int len = gps_data_ptr->rx_pmtk_len < n ? gps_data_ptr->rx_pmtk_len : n;
	memcpy(str, gps_data_ptr->rx_pmtk_buf, len);	//  ����� �� ������� PMTK 
	gps_data_ptr->rx_pmtk_len = 0;
	res = len;
    }
    return res;
}


/**
 * NMEA ���� �����?
 */
#pragma section("FLASH_code")
u8 gps_nmea_exist(void)
{
    return gps_data_ptr->rx_fin;
}

/**
 * ����������� ������������� ������ ��� ������
 */
#pragma section("FLASH_code")
bool gps_check_for_reliability(void)
{
    return gps_data_ptr->rx_check;
}


/**
 * �������� ����� �� NMEA
 */
section("L1_code")
int gps_get_nmea_time(void)
{
    int res = 0;

    if (gps_data_ptr != NULL && gps_data_ptr->rx_check) {
	res = gps_data_ptr->rx_nmea_time;
    }
    return res;
}


#pragma section("FLASH_code")
s64 gps_get_dif_time(void)
{
    return gps_data_ptr->dif_time;
}


/**
 * ��������� ������ �� NMEA, ����� �� ������ ��� � ISR
 * $GPRMC,105105.000,A,5541.7967,N,03721.3407,E,0.39,0.00,261212,10.0,E,A*37
 * ������ � ����� �� ������ � ���������� ������� ������� � ���������
 */
#pragma section("FLASH_code")
static long gps_parse_nmea_string(void)
{
    TIME_DATE td;
    char buf[16];
    s64 t0, t1;
    long count = 0, res = 0;
    u8 i;

    gps_data_ptr->dif_time = 0;
    t0 = get_long_time();

    do {
	u8 ver_pos = 0, lon_pos = 0, lat_pos = 0, tim_pos = 0, dat_pos = 0, ns_pos = 0, we_pos = 0;
	u8 lat_wid, lon_wid;


	// ������� �� ������
	// $GPRMC,115406.000,A,5541.7843,N,03721.3629,E,0.02,0.00,031213,10.0,E,A*37
	for (i = 0; i < NMEA_GPRMC_STRING_SIZE; i++) {

	    if (gps_data_ptr->rx_buf_out[i] == 0x2c)
		count++;

	    // ������ ������� time
	    if (!tim_pos && count == 1) {
		tim_pos = i + 1;
	    } else if (!ver_pos && count == 2) {
		ver_pos = i + 1;	// ������ ������� ������������� 'A'
	    } else if (!lat_pos && count == 3) {
		lat_pos = i + 1;	// ������ ������� ������
	    } else if (!ns_pos && count == 4) {
		ns_pos = i + 1;	// ������� �������� N ��� S
		lat_wid = ns_pos - lat_pos - 1;
	    } else if (!lon_pos && count == 5) {
		lon_pos = i + 1;	// ������ ������� �������
	    } else if (!we_pos && count == 6) {
		we_pos = i + 1;	// ������� �������� W ��� E
		lon_wid = we_pos - lon_pos - 1;
	    } else if (!dat_pos && count == 9) {
		dat_pos = i + 1;	// ������� ����
		break;
	    }
	}
	if (count != 9 || gps_data_ptr->rx_buf_out[ver_pos] != 'A') {
	    gps_data_ptr->rx_check = false;
	    break;		// ��� �������������
	}
	// �����
	memcpy(buf, gps_data_ptr->rx_buf_out + tim_pos, 2);
	buf[2] = 0;
	td.hour = atoi(buf);

	memcpy(buf, gps_data_ptr->rx_buf_out + tim_pos + 2, 2);
	buf[2] = 0;
	td.min = atoi(buf);

	memcpy(buf, gps_data_ptr->rx_buf_out + tim_pos + 4, 2);
	buf[2] = 0;
	td.sec = atoi(buf);

	memcpy(buf, gps_data_ptr->rx_buf_out + dat_pos, 2);
	buf[2] = 0;
	td.day = atoi(buf);	/* Day of the month - [1,31] */

	memcpy(buf, gps_data_ptr->rx_buf_out + dat_pos + 2, 2);
	buf[2] = 0;
	td.mon = atoi(buf);	/* Months since January - [0,11] */

	memcpy(buf, gps_data_ptr->rx_buf_out + dat_pos + 4, 2);
	buf[2] = 0;
	td.year = atoi(buf) + 2000;	/* Years since 1900 */

	count = td_to_sec(&td);
	if (count > 0) {
	    gps_data_ptr->rx_check = true;
	    res = count;
	}
	// ������ � ������� ��������. ��� � 4 ������� 
	if (lat_wid < 2 || lat_wid > 12 || lon_wid < 2 || lon_wid > 12 || count % 4 != 0)
	    break;

	// $GPRMC,115406.000,A,5541.7840,N,03721.3629,E,0.02,0.00,031213,10.0,E,A*37
	// ��������� �����
	memcpy(buf, gps_data_ptr->rx_buf_out + lat_pos, lat_wid);
	buf[lat_wid] = 0;
	if (gps_data_ptr->rx_buf_out[ns_pos] == 'N') {
	    gps_data_ptr->lat = (atof(buf) * 10000.0);
	} else if (gps_data_ptr->rx_buf_out[ns_pos] == 'S') {
	    gps_data_ptr->lat = (-atof(buf) * 10000.0);
	} else {
	    break;
	}

	memcpy(buf, gps_data_ptr->rx_buf_out + lon_pos, lon_wid);
	buf[lon_wid] = 0;
	if (gps_data_ptr->rx_buf_out[we_pos] == 'E')
	    gps_data_ptr->lon = (atof(buf) * 10000.0);
	else if (gps_data_ptr->rx_buf_out[we_pos] == 'W')
	    gps_data_ptr->lon = (-atof(buf) * 10000.0);
	else
	    break;
    } while (0);

    t1 = get_long_time();
    gps_data_ptr->dif_time = t1 - t0;	// � ������������

    return res;
}


/**
 * �������� ������ � �������
 */
#pragma section("FLASH_code")
void gps_get_coordinates(s32 * lat, s32 * lon)
{
    int t0;

    if (gps_data_ptr) {
	t0 = get_sec_ticks();
	do {
	    *lat = gps_data_ptr->lat;
	    *lon = gps_data_ptr->lon;
	    if (*lat != 0 && *lon != 0) {
		break;
	    }
	    LED_blink();
	} while (get_sec_ticks() - t0 < TIM4_BUF_SIZE / 4);	/* ���� 4 ������� */
    }
}

/* ��������: GPRMC  ONLY! */
#pragma section("FLASH_code")
void gps_set_grmc(void)
{
    char str[NMEA_PMTK_STRING_SIZE * 2];	/* ��� ������ � 2 ���� ������! */
    int res;
    int t0 = get_sec_ticks();

    do {
	strcpy(str, "$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29\r\n");
	log_write_log_file("INFO: Reprogram UART0 to receive $GPRMC string: %s\n", str);
	res = UART0_write_str(str, strlen(str));
	if (res < 0) {
	    log_write_log_file("ERROR: UART0 is not open. Can't set GPRMC\n");
	    break;
	}

	res = -1;
	memset(str, 0, sizeof(str));
	do {
	    if (gps_get_pmtk_string(str, NMEA_PMTK_STRING_SIZE) > 0) {	// ������� PMTK ������ ���� ��� ����
		log_write_log_file("INFO: %s\n", str);
		res = 0;
		break;
	    }
	    LED_blink();
	} while (get_sec_ticks() - t0 < TIM4_BUF_SIZE / 4);
	if (res < 0) {
	    log_write_log_file("INFO: Reprogram UART0 to receive $GPRMC string: can't get reply\n");
	}

    } while (0);

}


/**
 * ���������� 
 */
#pragma section("FLASH_code")
void gps_wake_up(void)
{
    char str[NMEA_PMTK_STRING_SIZE];
    char *p;
    int res = -1;
    int t0 = get_sec_ticks();

    do {
	strcpy(str, "$PMTK000*32\r\n");
	log_write_log_file("INFO: Try to WakeUp: %s\n", str);
	res = UART0_write_str(str, strlen(str));
	if (res < 0) {
	    log_write_log_file("ERROR: UART0 is not open. Can't send command for wakeup\n");
	    break;
	}

	/* ���� 4 �������: $PMTK000 */
	res = -1;
	memset(str, 0, sizeof(str));
	do {
	    if (gps_get_pmtk_string(str, NMEA_PMTK_STRING_SIZE) > 0) {	// ������� PMTK ������ ���� ��� ����
		log_write_log_file("INFO: %s\n", str);
		p = strstr(str, "$PMTK");	// ����� ������
		if (p != NULL) {
		    res = 0;
		    break;
		}
	    }
	    LED_blink();
	} while (get_sec_ticks() - t0 < TIM4_BUF_SIZE / 4);

	if (res < 0) {
	    log_write_log_file("INFO: Can't get reply or command not supported: %s\n", str);
	}
    } while (0);
}


/**
 * �������
 */
#pragma section("FLASH_code")
void gps_standby(void)
{
    char str[NMEA_PMTK_STRING_SIZE];
    char *p;
    int res;
    int t0 = get_sec_ticks();

    do {
	strcpy(str, "$PMTK161,0*28\r\n");
	log_write_log_file("INFO: Try to StandBy: %s\n", str);
	res = UART0_write_str(str, strlen(str));
	if (res < 0) {
	    log_write_log_file("ERROR: UART0 is not open. Can't send command for StandBy\n");
	    break;
	}

	/* ���� 4 �������: $PMTK*** */
	res = -1;
	memset(str, 0, sizeof(str));
	do {
	    if (gps_get_pmtk_string(str, NMEA_PMTK_STRING_SIZE) > 0) {	// ������� PMTK ������ ���� ��� ����
		log_write_log_file("INFO: %s\n", str);
		p = strstr(str, "$PMTK");	// ����� ������
		if (p != NULL) {
		    res = 0;
		    break;
		}
	    }
	    LED_blink();
	} while (get_sec_ticks() - t0 < TIM4_BUF_SIZE / 4);

	if (res < 0) {
	    log_write_log_file("ERROR: Can't get reply or command not supported\n");
	}
    } while (0);
}



/* ���������� �������� ������� UTC */
#pragma section("FLASH_code")
int gps_get_utc_offset(void)
{
    char str[NMEA_PMTK_STRING_SIZE];
    char *p, *p1;
    int res;
    int t0 = get_sec_ticks();

    do {
	strcpy(str, "$PMTK457*34\r\n");
	res = UART0_write_str(str, strlen(str));
	log_write_log_file("INFO: Try to get UTC offset: %s\n", str);
	if (res < 0) {
	    log_write_log_file("ERROR: UART0 is not open. Can't get UTC offset\n");
	    break;
	}

	/* ���� 4 �������: $PMTK001,457,1*34 ��� $PMTK557,15.0*03 */
	res = -1;
	memset(str, 0, sizeof(str));
	do {
	    if (gps_get_pmtk_string(str, NMEA_PMTK_STRING_SIZE) > 0) {	// ������� PMTK ������ ���� ��� ����
		log_write_log_file("INFO: %s\n", str);

		/* ������ ������ Sylvana */
		p = strstr(str, "$PMTK557,");	/* ����� ������ */
		if (p != NULL) {
		    memcpy(str, p + 9, 2);
		    str[2] = 0;
		    res = atoi(str);
/*    log_write_log_file("INFO: UTC Offset: %d sec\n", res); */
		    break;
		}

 		/* ����� ������ FastRax */
		p1 = strstr(str, "$PMTK001,457");
		if (p1 != NULL) {
/*		    log_write_log_file("INFO: UTC Offset: assume 16 sec\n"); */
		    res = 16;
		    break;
		}
	    }
	    LED_blink();
	} while (get_sec_ticks() - t0 < TIM4_BUF_SIZE / 4);

    } while (0);

    if (res < 0) {
	log_write_log_file("INFO: Can't get reply or command not supported\n");
    }
  return res;
}


#pragma section("FLASH_code")
void gps_set_zda(void)
{
    char str[NMEA_PMTK_STRING_SIZE];
    int res, t0;
    char *p;


    do {
	/* ��������: GPRMC, -, GPGGA, GPGSA, GPGSV, + GPZDA */
	strcpy(str, "$PMTK314,0,1,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,0*29\r\n");
	log_write_log_file("INFO: Reprogram UART0 to receive $GPZDA string: %s\n", str);

	res = UART0_write_str(str, strlen(str));
	if (res < 0) {
	    log_write_log_file("ERROR: UART0 is not open. Can't send command for wakeup\n");
	    break;
	}


#if 1
	/* ���� 2 �������: $PMTK */
	res = -1;
	t0 = get_sec_ticks();
	memset(str, 0, sizeof(str));
	do {
	    if (gps_get_pmtk_string(str, NMEA_PMTK_STRING_SIZE) > 0) {	// ������� PMTK ������ ���� ��� ����
		log_write_log_file("INFO: %s\n", str);
		p = strstr(str, "$PMTK");	// ����� ������
		if (p != NULL) {
		    res = 0;
		    break;
		}
	    }
	    LED_blink();
	} while (get_sec_ticks() - t0 < TIM4_BUF_SIZE / 8);
#endif
    } while (0);

    if (res < 0) {
	log_write_log_file("INFO: Can't get reply or command not supported\n");
    }
}



/* �������� �������� */
#pragma section("FLASH_code")
int gps_change_baud(void)
{
    DEV_UART_STRUCT gps_par;
    char str[NMEA_PMTK_STRING_SIZE];
    char *p;
    int res = -1;
    int t0 = get_sec_ticks();

    do {
	strcpy(str, "$PMTK251,9600*17\r\n");
	log_write_log_file("INFO: Try to change baud: %s\n", str);
	res = UART0_write_str(str, strlen(str));
	if (res < 0) {
	    log_write_log_file("ERROR: UART0 is not open. Can't send command for wakeup\n");
	    break;
	}

	/* ���� 2 �������: $PMTK */
	res = -1;
	memset(str, 0, sizeof(str));
	do {
	    if (gps_get_pmtk_string(str, NMEA_PMTK_STRING_SIZE) > 0) {	// ������� PMTK ������ ���� ��� ����
		log_write_log_file("INFO: %s\n", str);
		p = strstr(str, "$PMTK");	// ����� ������
		if (p != NULL) {
		    res = 0;
		    break;
		}
	    }
	    LED_blink();
	} while (get_sec_ticks() - t0 < TIM4_BUF_SIZE / 8);

    } while (0);

    if (res < 0) {
	log_write_log_file("INFO: Can't get reply or command not supported\n");
    }

    /* ����������������� UART0 */
    gps_par.baud = GPS_NMEA_NEW_SPEED;
    gps_par.rx_call_back_func = gps_nmea_read_ISR;
    gps_par.tx_call_back_func = NULL;	/* ���  */
    if (UART0_init(&gps_par) == true)
	res = 0;

    return res;
}
