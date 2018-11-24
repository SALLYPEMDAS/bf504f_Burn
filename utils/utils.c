/******************************************************************************
 * ������� �������� ���, �������� ����������� ����� �.�. ����� 
 * ��� ������� ������� ����� �� ������ ����� (01-01-1970)
 * ��� ������� � ��������� �����
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "main.h"
#include "timer1.h"
#include "timer2.h"
#include "timer4.h"
#include "utils.h"
#include "eeprom.h"
#include "timer1.h"
#include "timer2.h"
#include "ads1282.h"
#include "lsm303.h"
#include "math.h"
#include "dac.h"
#include "log.h"
#include "rsi.h"


section("FLASH_data")
static const char *monthes[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

#define 	M_PI				3.141592653589
#define		DAC_TUNE_COEF		 	94

static f32 AccSensorNorm(f32 *);

/******************************************************************************* 
 * ��������� ������� ����� � ��� � ������� � ������ �����
 * int t_year,t_mon,t_day; - ������� ����
 * int t_hour,t_min,t_sec; - ������� �����
 * ����� ��������� �� FLASH - �� ��������!
 *******************************************************************************/
#pragma section("FLASH_code")
long td_to_sec(TIME_DATE * t)
{
    long r;
    struct tm tm_time;

    tm_time.tm_sec = t->sec;
    tm_time.tm_min = t->min;
    tm_time.tm_hour = t->hour;
    tm_time.tm_mday = t->day;
    tm_time.tm_mon = t->mon - 1;	/* � TIME_DATE ������ � 1 �� 12, � tm_time � 0 �� 11 */
    tm_time.tm_year = t->year - 1900;	/* � tm ���� ��������� � 1900 - �� */
    tm_time.tm_wday = 0;	/* �� ������������ */
    tm_time.tm_yday = 0;
    tm_time.tm_isdst = 0;

    /* ������ ������� */
    r = mktime(&tm_time);
    return r;			/* -1 ��� ���������� �����  */
}


/***********************************************************************************
 * ������ ���� � �������
 * ����� ��������� �� FLASH - �� ��������!
 ***********************************************************************************/
#pragma section("FLASH_code")
long get_min_ticks(void)
{
    return (long) ((((u64) get_long_time() / TIMER_NS_DIVIDER) + 1) / 60);	/* �� ������������!!!  */
}




/***********************************************************************************  
 * �������� ������ � ��������
 * ����� ��������� �� FLASH - �� ��������!
 **********************************************************************************/
#pragma section("FLASH_code")
void sec_to_str(u32 ls, char *str)
{
    TIME_DATE t;

    /* ���������� ���� � �������: 08-11-12 - 08:57:22 */
    if (sec_to_td(ls, &t) != -1) {
	sprintf(str, "%02d-%02d-%04d - %02d:%02d:%02d", t.day, t.mon, t.year, t.hour, t.min, t.sec);
    } else {
	sprintf(str, "[set time error]");
    }
}



/***********************************************************************************  
 * ���������� ���� � �������: 08-11-12 - 08:57:22
 **********************************************************************************/
#pragma section("FLASH_code")
void td_to_str(TIME_DATE * t, char *str)
{
    sprintf(str, "%02d-%02d-%04d - %02d:%02d:%02d", t->day, t->mon, t->year, t->hour, t->min, t->sec);
}


/***********************************************************************************
 * ���������� ����� �� ����� ��������
 * ����� ��������� �� FLASH - �� ��������!
 ***********************************************************************************/
#pragma section("FLASH_code")
void set_sec_ticks(long sec)
{
    TIMER1_set_sec(sec);
    TIMER2_set_sec(sec);
}


/***********************************************************************************  
 * ����������� � ������
 **********************************************************************************/
#pragma section("FLASH_code")
void nsec_to_str(u64 ls, char *str)
{
    u32 sec, nsec;
    TIME_DATE t;

    sec = (u64) ls / TIMER_NS_DIVIDER;
    nsec = (u64) ls % TIMER_NS_DIVIDER;

    /* ���������� ���� � �������: 08-11-12 - 08:57:22 */
    if (sec_to_td(sec, &t) != -1) {
	sprintf(str, "%02d-%02d-%04d - %02d:%02d:%02d.%09d", t.day, t.mon, t.year, t.hour, t.min, t.sec, nsec);
    } else {
	sprintf(str, "[set time error] ");
    }
}


/***********************************************************************************
 * ����� � ���������� ENDIAN: 1 - Big-endian, 0 - Little-endian. � �������� �������� 
 ***********************************************************************************/
#pragma section("FLASH_code")
int get_cpu_endian(void)
{
    int i = 1;
    if (*((u8 *) & i) == 0)	/* Big-endian */
	return 1;
    else
	return 0;		/* Little-endian */
}


/**
 * �������� ����� ��������, �������� ��� �� �� ����� �� ��������� �����
 */
#pragma section("FLASH_code")
u32 get_buf_sum(u32 * buf, int size)
{
    int i;
    u32 sum = 0;

    if (buf != NULL & size != 0) {

	for (i = 0; i < size; i++)
	    sum += buf[i];
    }
    return sum;
}


/**
 * �������� ������� �������� ������
 */
#pragma section("FLASH_code")
u32 get_buf_average(u32 * buf, int size)
{
    int i;
    u32 sum = 0;

    if (buf != NULL & size != 0) {
	for (i = 0; i < size; i++)
	    sum += buf[i];
	sum /= size;
    }
    return sum;
}


/**
 * �������� ����������� �������� ��� ������ �� ��� ����������
 * �� ������� ������ ����� 2 �����
 * ��� 16-�� ������
 */
#pragma section("FLASH_code")
u16 get_dac_ctrl_value(u32 sum, u16 old_dac, u16 delta)
{
    long err;
    int new_dac;
    int coef = (delta < 10 || delta > 250) ? 98 : delta;

    err = sum - TIM4_TICKS_FOR_16_SEC;

    // ���. �����:    new_dac = (-err) * 16384 / DAC_TUNE_COEF / 16 + old_dac;
    new_dac = (-err) * 1024 / coef + old_dac;

    /* ������ ������������, ��� �������� �� ������� ��������� ��������!!! */
    if (new_dac > DAC19_MAX_DATA)
	new_dac = DAC19_MAX_DATA;
    if (new_dac < DAC19_MIN_DATA)
	new_dac = DAC19_MIN_DATA;

    return (u16) new_dac;
}


/**
 * �������� ����������� �������� ��� ������ �� ��� ����������
 * �� ������� ������ ����� 2 �����
 */
#pragma section("FLASH_code")
u16 get_dac_ctrl_value_old(u32 * buf, int size, u16 old_dac, u16 delta)
{
    long err;
    int new_dac;
    int coef = (delta < 10 || delta > 250) ? 98 : delta;

    err = get_buf_sum(buf, size) - SCLK_VALUE * size;

    // ���. �����:    new_dac = (-err) * 16384 / DAC_TUNE_COEF / 16 + old_dac;
    new_dac = (-err) * 1024 / coef + old_dac;

    /* ������ ������������, ��� �������� �� ������� ��������� ��������!!! */
    if (new_dac > DAC19_MAX_DATA)
	new_dac = DAC19_MAX_DATA;
    if (new_dac < DAC19_MIN_DATA)
	new_dac = DAC19_MIN_DATA;

    return (u16) new_dac;
}



/* ������ � ������� �������   */
#pragma section("FLASH_code")
void str_to_cap(char *str, int len)
{
    int i = len;

    while (i--)
	str[i] = (str[i] > 0x60) ? str[i] - 0x20 : str[i];
}


//Mar 14 2013 13:53:31
#pragma section("FLASH_code")
bool parse_date_time(char *str_date, char *str_time, TIME_DATE * time)
{
    char buf[5];
    int i, len, x;

    len = strlen(str_date);
    str_to_cap(str_date, len);	// � ������� �������

    for (i = 0; i < len; i++)
	if (isalpha(str_date[i]))	// ������ ������ �����
	    break;

    if (i >= len)
	return false;


    memset(buf, 0, 5);
    strncpy(buf, str_date + i, 3);	// 3 ������� �����������
    for (i = 0; i < 12; i++) {
	if (strncmp(buf, monthes[i], 3) == 0) {
	    time->mon = i + 1;	// ����� �����
	    break;
	}
    }
    if (time->mon > 12)
	return false;


    // ���� ������ �����
    for (i = 0; i < len; i++)
	if (isdigit(str_date[i])) {
	    x = i;
	    break;
	}

    memset(buf, 0, 5);
    strncpy(buf, str_date + i, 2);	// 2 �������
    time->day = atoi(buf);

    if (time->day > 31)
	return false;

    // ���� ���, ������ ����� ����
    for (i = x; i < len; i++) {
	if (str_date[i] == 0x20) {
	    break;
	}
    }
    strncpy(buf, str_date + i + 1, 4);	// 4 �������
    time->year = atoi(buf);

    if (time->year < 2013)
	return false;


    /* 3... ��������� ������ ������� */
    memset(buf, 0, 5);

    /* ���� */
    strncpy(buf, str_time, 2);
    time->hour = atoi(buf);

    if (time->hour > 24)
	return false;


    /* ������ */
    strncpy(buf, str_time + 3, 2);
    time->min = atoi(buf);
    if (time->min > 60)
	return false;


    /* ������� */
    strncpy(buf, str_time + 6, 2);
    time->sec = atoi(buf);
    if (time->sec > 60)
	return false;

    return true;
}


/**
 * �������� ��� �������, ���� ����� ������ ����� ��� �������
 */
#pragma section("FLASH_code")
int check_start_time(void *par)
{
    char str[32];
    GNS110_PARAM_STRUCT *time;
    long t0;
    int res = -1;

    do {
	if (par == NULL) {
	    break;
	}
	time = (GNS110_PARAM_STRUCT *) par;	/* �������� ���������  */
	t0 = get_sec_ticks();

	/* ������ ���� ����. ������ � ������ - ��������� �� ����� ������   */
	if (time->gns110_start_time <= (t0 + WAIT_TIM3_SYNC)) {
	    time->gns110_start_time = t0 - (t0 % 60) + WAIT_PC_TIME;	/* �� 2 ������ ����� "������" */
 	    sec_to_str(t0, str);
            log_write_log_file("INFO: start time was shifted on %s\n", str);
	}

	/* ���� ����� ��������� ���������� */
	if (time->gns110_finish_time <= (int) time->gns110_start_time) {
	    log_write_log_file("ERROR: start time can't be more finish time\n");
	    break;		/*  ������ */
	}
	res = 0;
    } while (0);
    return res;
}

/**
 * �������� ������ �����
 */
#pragma section("FLASH_code")
void print_adc_data(void *par)
{
    log_write_env_data_to_file(par);
}


/**
 * �������� ������
 */
#pragma section("FLASH_code")
void print_status(void *par)
{
    ADS1282_Regs regs;
    DEV_DAC_STRUCT dac;
    DEV_STATUS_STRUCT *status;
    char str[32];

    if (par == NULL)
	return;

    status = (DEV_STATUS_STRUCT *) par;


    log_write_log_file(">>>>>>>>>>>>>>>Status and Tests<<<<<<<<<<<<<<<\n");
    log_write_log_file("INFO: %s\n", status->st_main & 0x01 ? "No time in RTC" : "Time OK");
/*    log_write_log_file("INFO: %s\n", status->st_main & 0x02 ? "No const in EEPROM" : "Const OK"); */
    log_write_log_file("INFO: %s\n", status->st_test0 & 0x01 ? "RTC error" : "RTC OK");
    log_write_log_file("INFO: %s\n", status->st_test0 & 0x02 ? "T & P error" : "T & P OK");
    log_write_log_file("INFO: %s\n", status->st_test0 & 0x04 ? "Accel & comp error" : "Accel & comp  OK");
    log_write_log_file("INFO: %s\n", status->st_test0 & 0x08 ? "Modem error" : "Modem OK");
    log_write_log_file("INFO: %s\n", status->st_test0 & 0x10 ? "GPS error" : "GPS OK");
    log_write_log_file("INFO: %s\n", status->st_test0 & 0x20 ? "EEPROM error" : "EEPROM OK");


#if QUARTZ_CLK_FREQ==(19200000)
    log_write_log_file("INFO: %s\n", status->st_test1 & 0x10 ? "Quartz 19.2  MHz error" : "Quartz 19.2  MHz OK");
    log_write_log_file("INFO: %s\n", status->st_test1 & 0x20 ? "Quartz 4.096 MHz error" : "Quartz 4.096 MHz OK");
    log_write_log_file("INFO: %s\n", status->st_test1 & 0x80 ? "TIMER3 error" : "TIMER3  OK");
#else
    log_write_log_file("INFO: %s\n", status->st_test1 & 0x10 ? "Quartz 8.192  MHz error" : "Quartz 8.192  MHz OK");
#endif
    log_write_log_file("INFO: %s\n", status->st_test1 & 0x40 ? "TIMER4 error" : "TIMER4 OK");



    read_dac_coefs_from_eeprom(&dac);
    read_ads1282_coefs_from_eeprom(&regs);
    log_write_log_file(">>>>>>>>>>>EEPROM status: %08X <<<<<<<<<<<<<<<\n", status->eeprom);
    log_write_log_file("INFO: EEPROM_MOD_ID:\t\t%d\n", read_mod_id_from_eeprom());
    log_write_log_file("INFO: EEPROM_RSVD0:\t\t%d\n", read_rsvd0_from_eeprom());
    log_write_log_file("INFO: EEPROM_RSVD1:\t\t%d\n", read_rsvd1_from_eeprom());
    log_write_log_file("INFO: EEPROM_TIME_WORK:\t%d\n", read_time_work_from_eeprom());
    log_write_log_file("INFO: EEPROM_TIME_CMD:\t%d\n", read_time_cmd_from_eeprom());
    log_write_log_file("INFO: EEPROM_TIME_MODEM:\t%d\n", read_time_modem_from_eeprom());
    log_write_log_file("INFO: EEPROM_DAC19_COEF:\t%d\n", dac.dac19_data);
    log_write_log_file("INFO: EEPROM_DAC4_COEF:\t%d\n", dac.dac4_data);
    log_write_log_file("INFO: EEPROM_RSVD1:\t\t%d\n", read_rsvd2_from_eeprom());
    log_write_log_file("INFO: EEPROM_ADC_OFS0:\t0x%08X\n", regs.chan[0].offset);
    log_write_log_file("INFO: EEPROM_ADC_FSC0:\t0x%08X\n", regs.chan[0].gain);
    log_write_log_file("INFO: EEPROM_ADC_OFS1:\t0x%08X\n", regs.chan[1].offset);
    log_write_log_file("INFO: EEPROM_ADC_FSC1:\t0x%08X\n", regs.chan[1].gain);
    log_write_log_file("INFO: EEPROM_ADC_OFS2:\t0x%08X\n", regs.chan[2].offset);
    log_write_log_file("INFO: EEPROM_ADC_FSC2:\t0x%08X\n", regs.chan[2].gain);
    log_write_log_file("INFO: EEPROM_ADC_OFS3:\t0x%08X\n", regs.chan[3].offset);
    log_write_log_file("INFO: EEPROM_ADC_FSC3:\t0x%08X\n", regs.chan[3].gain);

    // �� ���� ��������� ���������� reset
    if (status->st_reset == 1) {
	log_write_log_file("INFO: Last reset was:\tPOWER OFF\n");
    } else if (status->st_reset == 2) {
	log_write_log_file("INFO: Last reset was:\tEXT. RESET\n");
    } else if (status->st_reset == 4) {
	log_write_log_file("INFO: Last reset was:\tBURN OUT\n");
    } else if (status->st_reset == 8) {
	log_write_log_file("INFO: Last reset was:\tWDT RESET\n");
    } else if (status->st_reset == 16) {
	log_write_log_file("INFO: Last reset was:\tNO LINK POWER OFF\n");
    } else {
	log_write_log_file("INFO: Last reset was:\tUNKNOWN RESET\n");
    }
}

/**
 * ������� ��������� ���
 */
#pragma section("FLASH_code")
void print_ads1282_parms(void *v)
{
    GNS110_PARAM_STRUCT *gns110_param;
    u8 map;

    gns110_param = (GNS110_PARAM_STRUCT *) v;	/* �������� ���������  */


    log_write_log_file(">>>>>>>>>>ADS1282 parameters<<<<<<<<<\n");
    log_write_log_file("INFO: Sampling freq:  %####d\n", gns110_param->gns110_adc_freq);
    log_write_log_file("INFO: Mode:           %s\n", gns110_param->gns110_adc_consum == 1 ? "High res mode" : "Low power mode");
    log_write_log_file("INFO: PGA:            %####d\n", gns110_param->gns110_adc_pga);
    log_write_log_file("INFO: HPF freq:       %3.4f Hz\n", gns110_param->gns110_adc_flt_freq);


    /* ���� ����������� ��-�������� ��� ��� */
    if (gns110_param->gns110_adc_bitmap == 0) {
	log_write_log_file("ERROR: bitmap param's invalid\n");
	log_write_log_file("INFO: All channels will be turned on\n");
	gns110_param->gns110_adc_bitmap = 0x0f;	///// ���� ������ ==
    }

    log_write_log_file("INFO: Use channels:   4: %s, 3: %s, 2: %s, 1: %s\n",
		       (gns110_param->gns110_adc_bitmap & 0x08) ? "On" : "Off",
		       (gns110_param->gns110_adc_bitmap & 0x04) ? "On" : "Off",
		       (gns110_param->gns110_adc_bitmap & 0x02) ? "On" : "Off", (gns110_param->gns110_adc_bitmap & 0x01) ? "On" : "Off");
    log_write_log_file("INFO: File length %d hour(s)\n", gns110_param->gns110_file_len);
}

/**
 * ������� ��� �������� �������
 */
#pragma section("FLASH_code")
void print_set_times(void *par)
{
    char str[32];
    long t;
    GNS110_PARAM_STRUCT *time;

    if (par != NULL) {

	time = (GNS110_PARAM_STRUCT *) par;	/* �������� ���������  */

	log_write_log_file(">>>>>>>>>>>>>>>>  Times  <<<<<<<<<<<<<<<<<\n");
	t = get_sec_ticks();
	sec_to_str(t, str);
	log_write_log_file("INFO: now time is:\t\t%s\n", str);

	/* �� ���������� 1970-� ��� */
	if (time->gns110_start_time > 1) {
	    sec_to_str(time->gns110_start_time, str);
	    log_write_log_file("INFO: registration starts at:\t%s\n", str);
	}

	/* �� ���������� 1970-� ��� */
	if (time->gns110_finish_time > 1) {
	    sec_to_str(time->gns110_finish_time, str);
	    log_write_log_file("INFO: registration finish at:\t%s\n", str);
	}

	/* �� ���������� 1970-� ��� */
	if (time->gns110_burn_on_time > 1) {
	    sec_to_str(time->gns110_burn_on_time, str);
	    log_write_log_file("INFO: burn wire on at:\t%s\n", str);
	}
	/* �� ���������� 1970-� ��� */
	if (time->gns110_burn_off_time > 1) {
	    sec_to_str(time->gns110_burn_off_time, str);
	    log_write_log_file("INFO: burn wire off time at:\t%s\n", str);
	}
	sec_to_str(time->gns110_gps_time, str);
	log_write_log_file("INFO: turn on GPS at:\t\t%s\n", str);
    } else {
	log_write_log_file("ERROR: print times");
    }
}

/**
 * ������� ����� � ����� ������� � �������� � ������ ����� ������ �����
 * ����� ������ � ���� �������
 * �����  = �1 - T4
 */
#pragma section("FLASH_code")
void print_drift_and_work_time(s64 tim1, s64 tim4, s32 rtc, u32 wt)
{
    char str[64];
    TIME_DATE td;
    u32 sec, nsec;
    s64 drift;
    u8 sign = 0;

    /* �������� ����� T4 */
    nsec_to_str(tim4, str);
    log_write_log_file("INFO: GPS time:  %s\n", str);


    if (tim1 != 0) {
	// �������� ����� T1
	nsec_to_str(tim1, str);
	log_write_log_file("INFO: PRTC time: %s\n", str);

	// �������� ����� RTC
	sec_to_str(rtc, str);
	log_write_log_file("INFO: RTC  time: %s\n", str);

	// � ��� �����
	drift = tim1 - tim4;
	if (drift < 0) {
	    sign = 1;		// ���� ������������� �����
	    drift = 0 - drift;
	}
	// ���� ������ 1-� �������
	sec = (u64) drift / TIMER_NS_DIVIDER;

	if (abs(sec) < 1) {
	    drift = TIMER1_get_drift(NULL, NULL);
	    drift = abs(drift);
	}

	nsec = (u64) drift % TIMER_NS_DIVIDER;
	sec_to_td(sec, &td);
	if (wt > 0)
	    log_write_log_file("INFO: Drift final full: %c%02d days %02d:%02d:%02d.%09d\n", sign ? '-' : ' ', td.hour / 24, td.hour % 24, td.min, td.sec, nsec);
	else
	    log_write_log_file("INFO: Drift begin full: %c%02d days %02d:%02d:%02d.%09d\n", sign ? '-' : ' ', td.hour / 24, td.hour % 24, td.min, td.sec, nsec);
    }

    if (wt > 0) {
	float div = (float) drift / wt;
	log_write_log_file("INFO: PRTC timer walks on %4.5f ns per sec\n", div);
	log_write_log_file("INFO: GNS worked %d sec\n", wt);
    }
}



#pragma section("FLASH_code")
void print_reset_cause(u32 cause)
{
    switch (cause) {

    case CAUSE_POWER_OFF:
	log_write_log_file("INFO: Disconnection reason - Power OFF\n");
	break;

    case CAUSE_EXT_RESET:
	log_write_log_file("INFO: Disconnection reason - Extern. reset\n");
	break;

    case CAUSE_BROWN_OUT:
	log_write_log_file("INFO: Disconnection reason - Brown out\n");
	break;

    case CAUSE_WDT_RESET:
	log_write_log_file("INFO: Disconnection reason - WDT reset\n");
	break;

    case CAUSE_NO_LINK:
	log_write_log_file("INFO: Disconnection reason - No link (> 10 min)\n");
	break;

    case CAUSE_UNKNOWN_RESET:
    default:
	log_write_log_file("INFO: Disconnection reason - Unknown reset\n");
	break;
    }
}


/**
 * �������� ������ ���� ��� ����
 */
#pragma section("FLASH_code")
void print_timer_and_sd_card_error(void)
{
    long sa, bs, err;
    SD_CARD_ERROR_STRUCT ts;
    ADS1282_ERROR_STRUCT ader;

    /* ������ ��������� � ������������ �����  */
    err = TIMER1_get_error();
    log_write_log_file("INFO: %d timer1 warning(s) were found for work time\n", err);

    /* ������ ��� - ������ �� �����  */
    ADS1282_get_error_count(&ader);
    log_write_log_file("INFO: %d ADC miss(es) were found\n", ader.sample_miss);	/* ������ ������ �� ������� */
    log_write_log_file("INFO: %d write error(s) were found on SD card\n", ader.block_timeout);	/* ���� �� ����� ������� ���������� */

    /* ������ sd ����� */
    rsi_get_card_timeout(&ts);
    log_write_log_file("INFO: %d command error(s) occured on SD Card\n", ts.cmd_error);
    log_write_log_file("INFO: %d read timeout(s) occured on SD Card\n", ts.read_timeout);
    log_write_log_file("INFO: %d write timeout(s) occured on SD Card\n", ts.write_timeout);
    log_write_log_file("INFO: %d other error(s) occured on SD Card\n", ts.any_error);

    /* ��� EEPROM �� Flash */
    eeprom_get_status(&err, &sa, &bs);
    log_write_log_file("INFO: EEPROM was formated %d time(s)\n", err);
    log_write_log_file("INFO: PAGE0 was erased %d, PAGE1 was erased %d time(s)\n", sa, bs);
}


/**
 * ������� ����� SD
 */
#pragma section("FLASH_code")
void get_sd_card_timeout(SD_CARD_ERROR_STRUCT * ts)
{
    if (ts != NULL)
	rsi_get_card_timeout(ts);
}



/**
 * ��������� ������������ �������� ������
 * ���� ���������� ����������� - �� ������� ����� ������
 */
#pragma section("FLASH_code")
int check_set_times(void *par)
{
    char str0[32], str1[32];
    long t;
    GNS110_PARAM_STRUCT *time;


    if (par == NULL)
	return -1;

    time = (GNS110_PARAM_STRUCT *) par;	/* �������� ���������  */

    t = get_sec_ticks();

    log_write_log_file("INFO: Checking times...\n");

#if 1
    /* ���� ����� "������" ������ ������� ������ �� ���� - �������� ���� �����! */
    if (abs((int) time->gns110_start_time - t) >= TIME_ONE_DAY) {
	log_write_log_file("ERROR: Time now can't be less start time on one day! Check RTC\n");
	return -1;
    }
#endif
    if (read_reset_cause_from_eeprom() == CAUSE_WDT_RESET) {
	if ((int) time->gns110_finish_time - t < TIME_START_AFTER_WAKEUP) {
	    log_write_log_file("ERROR: It hasn't time for adjust in WDT reset!\n");
	    return -1;
	} else {
	    log_write_log_file("INFO: Finish time set OK\n");
	}
    } else {
	if ((int) time->gns110_finish_time - (int) time->gns110_start_time < RECORD_TIME) {
	    log_write_log_file("ERROR: Record time less %d seconds!\n", RECORD_TIME);
	    return -1;
	} else {
	    log_write_log_file("INFO: Finish time set OK\n");
	}

	/* ������ ���� ������� 7 ����� �� ���������� � �����������  */
	if (((int) time->gns110_finish_time - t) < (RECORD_TIME + TIME_START_AFTER_WAKEUP)) {
	    sec_to_str(time->gns110_finish_time, str1);
	    log_write_log_file("INFO: Finish time: %s\n", str1);
	    log_write_log_file("INFO: Please correct finish time!\n", str1);
	    log_write_log_file("ERROR: Not enough time for tuning or time is over!\n");
	    return -1;
	} else {
	    sec_to_str(t, str0);
	    sec_to_str(time->gns110_finish_time, str1);
	    log_write_log_file("INFO: Time set OK\n");
	    log_write_log_file("INFO: Now (%s)\n", str0);
	    log_write_log_file("INFO: Fin (%s)\n", str1);
	}

	if ((int) time->gns110_burn_on_time <= (int) time->gns110_finish_time) {
	    log_write_log_file("ERROR: Burn-relay turn on time less finish time!\n");
	    return -1;
	} else {
	    log_write_log_file("INFO: Burn time set OK\n");
	}

	/* ����� �������� �� ������ � ���� �� ������ ������������� */
	if (abs((int) time->gns110_burn_on_time - (int) time->gns110_modem_alarm_time) < (RELEBURN_TIME * 2)) {
	    log_write_log_file("ERROR: Burn-relay time and modem alarm time can't overlap!\n");
	} else {
	    log_write_log_file("INFO: Burn-relay time and modem alarm set OK. Times don't overlap!\n");
	}

	if ((int) time->gns110_burn_off_time - (int) time->gns110_burn_on_time < RELEBURN_TIME) {
	    log_write_log_file("ERROR: Burnout time less %d seconds!\n", RELEBURN_TIME);
	    return -1;
	} else {
	    log_write_log_file("INFO: burn wire off set OK\n");
	}

	if ((int) time->gns110_gps_time - (int) time->gns110_burn_off_time < POPUP_DURATION) {	// � 1 ������!
	    log_write_log_file("ERROR: Turn on GPS time need to be %d minutes later burn wire off time!\n", POPUP_DURATION);
	    return -1;
	} else {
	    log_write_log_file("INFO: Turn on GPS time set OK\n");
	}
    }

    return 0;
}


/**
 * ��������� ��� ���������� ����� ��� ������
 */
#pragma section("FLASH_code")
int check_modem_times(void *v)
{
    int res = 0;
    char str[128];
    GNS110_PARAM_STRUCT *par;

    if (v == NULL)
	return -1;

    par = (GNS110_PARAM_STRUCT *) v;

    if (par->gns110_modem_type > 0) {
	long t0;

	t0 = get_sec_ticks();

	if (par->gns110_modem_alarm_time < t0) {
	    sec_to_str(par->gns110_modem_alarm_time, str);
	    log_write_log_file("ERROR: modem alarm time is over: %s!\n", str);
	    res = -1;
	} else {
	    log_write_log_file("INFO: modem alarm time set OK\n");
	}
    }

    return res;
}

/** 
 * ���������� ��� � ����� ������
 */
#pragma section("FLASH_code")
void print_modem_type(void *p)
{
    if (p != NULL) {

	log_write_log_file("INFO: modem type: %s, modem num: %d\n",
			   ((GNS110_PARAM_STRUCT *) p)->gns110_modem_type == GNS110_NOT_MODEM ? "not" :
			   ((GNS110_PARAM_STRUCT *) p)->gns110_modem_type == GNS110_MODEM_OLD ? "old" :
			   ((GNS110_PARAM_STRUCT *) p)->gns110_modem_type == GNS110_MODEM_AM3 ? "am3" : "benthos",
			   ((GNS110_PARAM_STRUCT *) p)->gns110_modem_num);
    }
}


/**
 * ����� � ����� ���������� � floating point
 * ���������� ��� ����� ����� ��������� FLOAT � DOUBLE (long double �� ��������������)
 */
#pragma section("FLASH_code")
void print_coordinates(s32 lat, s32 lon)
{
    int temp;

    temp = abs(lat);
    log_write_log_file("INFO: Latitude  --> %04d.%04d(%c)\n", temp / 10000, temp % 10000, lat >= 0 ? 'N' : 'S');

    temp = abs(lon);
    log_write_log_file("INFO: Longitude --> %04d.%04d(%c)\n", temp / 10000, temp % 10000, lon >= 0 ? 'E' : 'W');
}


/**
 * ������ ����� �������� � ��������
 * �� ����� ��������� �� ������ �������, �������������
 * �� ������ ��������� �� ���������� ���� � ����� (��������) * 10 -> � �������������
 */
#pragma section("FLASH_code")
bool calc_angles_new(lsm303_data * acc, lsm303_data * mag)
{

    f32 fTemp;
    f32 fSinRoll, fCosRoll, fSinPitch, fCosPitch;
    f32 fTiltedX, fTiltedY;
    f32 fAcc[3];		/* ������������ */
    f32 fMag[3];		/* ������ */
    bool res = false;


    if (acc != NULL && mag != NULL) {
	fAcc[0] = (f32) acc->x / 100.0;
	fAcc[1] = (f32) acc->y / 100.0;
	fAcc[2] = (f32) acc->z / 100.0;

	fMag[0] = (f32) mag->x;
	fMag[1] = (f32) mag->y;
	fMag[2] = (f32) mag->z;

	/* ������������� ������� ��������� */
	fTemp = AccSensorNorm(fAcc);

	/* ��������� ������������� ��������� ������� �������� */
	fSinRoll = fAcc[1] / sqrt(pow(fAcc[1], 2) + pow(fAcc[2], 2));
	fCosRoll = sqrt(1.0 - fSinRoll * fSinRoll);
	fSinPitch = -fAcc[0] * fTemp;
	fCosPitch = sqrt(1.0 - fSinPitch * fSinPitch);

	/* ������� �������� -> � ���������� ���� ��� ����������� X � Y */
	fTiltedX = fMag[0] * fCosPitch + fMag[2] * fSinPitch;
	fTiltedY = fMag[0] * fSinRoll * fSinPitch + fMag[1] * fCosRoll - fMag[2] * fSinRoll * fCosPitch;

	/* �������� ���� head  */
	fTemp = -atan2(fTiltedY, fTiltedX) * 180.0 / M_PI;
	acc->z = (int) (fTemp * 10.0);

	/* � ���� roll � pitch */
	fTemp = atan2(fSinRoll, fCosRoll) * 180 / M_PI;
	acc->x = (int) (fTemp * 10.0);

	fTemp = atan2(fSinPitch, fCosPitch) * 180 / M_PI;
	acc->y = (int) (fTemp * 10.0);
	res = true;
    }
    return res;
}

/**
 * ����������� ������� ���������, ��� ����, ����� ������� �� ������ G
 */
#pragma section("FLASH_code")
static f32 AccSensorNorm(f32 * pfAccXYZ)
{
    /* ������� ��������� ��. ����� */
    return inv_sqrt(pow(pfAccXYZ[0], 2) + pow(pfAccXYZ[1], 2) + pow(pfAccXYZ[2], 2));
}

/**
 * Fast inverse square-root  
 * See: http://en.wikipedia.org/wiki/Fast_inverse_square_root
 */
f32 inv_sqrt(f32 x)
{
    unsigned int i = 0x5F1F1412 - (*(unsigned int *) &x >> 1);
    f32 tmp = *(f32 *) & i;
    f32 y = tmp * (1.69000231 - 0.714158168 * x * tmp * tmp);
    return y;
}



/**
 * ���� ��������� � �������
 * �������� ������ � ��� � ������� ���������� ����������� ����!!!
 */
bool calc_angles(void *par0, void *par1)
{
    lsm303_data *acc, *comp;
    f32 p, r, h;
    f32 mx, my, mz;
    f32 cx, cy, cz;
    f32 ax, ay, az;
    f32 f, f0;
    bool res = false;

    if (par0 != NULL && par1 != NULL) {
	acc = (lsm303_data *) par0;
	comp = (lsm303_data *) par1;

	// ���������� �������������
	ax = acc->x / 1000.0;
	ay = acc->y / 1000.0;
	az = acc->z / 1000.0;

	// ���������� �������
	cx = comp->x / 1100.0;
	cy = comp->y / 1100.0;
	cz = comp->z / 980.0;

	// ������� pitch � � ax �� �������� ������
	f = -(ax);
	if (f >= 1.0)
	    f = 0.999999999;
	if (f <= -1.0)
	    f = -0.999999999;
	p = asin(f);

	// � r
	f = ay;
	if (f >= 1.0)
	    f = 0.999999999;
	if (f <= -1.0)
	    f = -0.999999999;

	f0 = cos(p);
	if (f0 == 0.0) {
	    f0 = 1.0;
	} else {
	    f0 = f / f0;	// error
	}

	if (f0 >= 1.0)
	    f0 = 0.999999999;
	if (f0 <= -1.0)
	    f0 = -0.999999999;
	r = asin(f0);

	mx = cx * cos(p) + cz * sin(p);
	my = cx * sin(r) * sin(p) + cy * cos(r) - cz * sin(r) * cos(p);
	mz = -cx * cos(r) * sin(p) + cy * sin(r) + cz * cos(r) * cos(p);


#if 0
	if (mx > 0 && my >= 0) {
	    h = atan(my / mx);
	} else if (mx < 0) {
	    h = M_PI + atan(my / mx);
	} else if (mx > 0 && my <= 0) {
	    h = 2 * M_PI + atan(my / mx);
	} else if (mx == 0 && my < 0) {
	    h = M_PI / 2;
	} else if (mx == 0 && my > 0) {
	    h = M_PI / 2 * 3;
	}
#else
	h = atan2(my, mx);
#endif

	p *= 180 / M_PI;
	r *= 180 / M_PI;
	h *= 180 / M_PI;

	/* 0...360 */
	if (h <= 0.0) {
	    h = 360 + h;
	}

	/* ������������ ������ �����  */
	acc->x = p * 10;
	acc->y = r * 10;
	acc->z = h * 10;
	res = true;
    }
    return res;
}




///////////////////////////////////////////////////////////////////////////////
// ��� ��� ������� ����������� �� L1
////////////////////////////////////////////////////////////////////////////////

/**
 * ��������� �������� ������� HPF � ����������� �� ������� �����
 */
section("L1_code")
u16 get_hpf_from_freq(f32 freq, int dr)
{
    f32 rad, s;
    u16 reg = 0xFFFF;

    if (dr <= 0)
	return reg;

    rad = 2 * M_PI * freq / dr;	/* ��� ������� 250 */

    if (rad == 1.0)
	return reg;

    s = 1 - 2 * (cos(rad) + sin(rad) - 1) / cos(rad);

    if (s < 0.0)
	return reg;		/* -1 ������  */

    s = (1 - sqrt(s)) * 65536;
    if (s < 65535.0)
	reg = (u16) (s + 0.5);

    return reg;
}

/**
 * ������ ���� - �������. ������� ����� �� ������
 */
section("L1_code")
u16 get_char_from_buf(void *buf, int pos)
{
    return *(u8 *) buf + pos;
}

/**
 * ������� ����� short
 */
section("L1_code")
u16 get_short_from_buf(void *buf, int pos)
{
    u16 res;
    memcpy(&res, (u8 *) buf + pos, 2);
    return res;
}

/**
 * ������� ����� long
 */
section("L1_code")
u32 get_long_from_buf(void *buf, int pos)
{
    u32 res;
    memcpy(&res, (u8 *) buf + pos, 4);
    return res;
}

/**
 * ������� ����� float
 */
section("L1_code")
f32 get_float_from_buf(void *buf, int pos)
{
    union {
	float f;
	u32 l;
    } f;

    memcpy(&f.l, (u8 *) buf + pos, 4);
    return f.f;
}

/**
 * ��������� ������� (time_t) � ������ ����� � ������ TIME_DATE
 */
section("L1_code")
int sec_to_td(long ls, TIME_DATE * t)
{
    struct tm *tm_ptr;

    if ((int) ls != -1) {
	tm_ptr = gmtime(&ls);

	/* ���������� ����, ��� ���������� */
	t->sec = tm_ptr->tm_sec;
	t->min = tm_ptr->tm_min;
	t->hour = tm_ptr->tm_hour;
	t->day = tm_ptr->tm_mday;
	t->mon = tm_ptr->tm_mon + 1;	/* � TIME_DATE ������ � 1 �� 12, � tm_time � 0 �� 11 */
	t->year = tm_ptr->tm_year + 1900;	/* � tm ���� ��������� � 1900 - ��, � TIME_DATA � 0-�� */
	return 0;
    } else
	return -1;
}

/**
 * ������ ������� �����
 */
section("L1_code")
s64 get_long_time(void)
{
    if (TIMER1_is_run())
	return (s64) TIMER1_get_long_time();
    else
	return (s64) TIMER2_get_long_time();
}


/**
 * ������ ���� � ��������
 */
section("L1_code")
long get_sec_ticks(void)
{
    return (long) ((u64) get_long_time() / TIMER_NS_DIVIDER);	/* ������ ������� */
}

/**
 * ������ ���� � ������������� (������� + ������������)
 */
section("L1_code")
s64 get_msec_ticks(void)
{
    return (s64) ((u64) get_long_time() / TIMER_US_DIVIDER);	/* ������ ������������ �� �� */
}


/**
 * ������ ���� � �������������
 */
section("L1_code")
s64 get_usec_ticks(void)
{
    return (s64) ((u64) get_long_time() / TIMER_MS_DIVIDER);	/* ������ ������������ �� �� */
}
