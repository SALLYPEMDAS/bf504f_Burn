/*******************************************************************************************************************
 * ������ � ������������ ���������� �� ������� ����� �������, 
 * ������� �������� � ��� ������ �� ����������� �������� 100 ���
 * SCL clock low time - 4.7 ��� �������
 * SCL clock hi  time - 4.0 ��� �������
 *******************************************************************************************************************/
#include <string.h>
#include "utils.h"
#include "lsm303.h"
#include "twi.h"
#include "log.h"


/* �������� �������������  */
#define 	LSM303_ACC_ADDR		0x19	/* -- � ������� ������������ �����. ���� ���������� */
#define 	CTRL_REG1_A		0x20
#define 	CTRL_REG4_A		0x23
#define         STATUS_REG_A            0x27	/* Status register acceleration */
#define 	OUT_X_L_A		0x28
#define 	OUT_X_H_A		0x29
#define 	OUT_Y_L_A		0x2a
#define 	OUT_Y_H_A		0x2b
#define 	OUT_Z_L_A		0x2c
#define 	OUT_Z_H_A		0x2d


/* �������� ������� */
#define 	LSM303_COMP_ADDR	0x1E
#define 	CRA_REG_M		0x00	/* data rate reg */
#define 	CRB_REG_M		0x01	/* GAIN reg - �������� */
#define		MR_REG_M		0x02	/* Mode sel reg - ����� ������ */

#define		OUT_X_H_M		0x03
#define		OUT_X_L_M		0x04
#define		OUT_Z_H_M		0x05
#define		OUT_Z_L_M		0x06
#define		OUT_Y_H_M		0x07
#define		OUT_Y_L_M		0x08
#define         SR_REG_M                0x09	/* Status Register magnetic field */

/* �������� ���������� */
#define 	TEMP_OUT_H_M 		0x31
#define 	TEMP_OUT_L_M 		0x32


/* ��� ������� */
#define 	LSM303_TWI_COMP_HI	36
#define 	LSM303_TWI_COMP_LO      37




/* ��� ������������� */
#define 	LSM303_TWI_ACC_HI	36
#define 	LSM303_TWI_ACC_LO       37


#define 	LSM303_NUM_MOD		2



#define         RES_DATA_LOCK		0x1FFF


/* ��������� ���������� TWI - ������������ */
section("FLASH_data")
static const twi_clock_div lsm303_acc_val = {
    .lo_div_clk = LSM303_TWI_ACC_LO,
    .hi_div_clk = LSM303_TWI_ACC_HI,
    .rsvd = LSM303_NUM_MOD
};


section("FLASH_data")
static const twi_clock_div lsm303_comp_val = {
    .lo_div_clk = LSM303_TWI_COMP_LO,
    .hi_div_clk = LSM303_TWI_COMP_HI,
    .rsvd = LSM303_NUM_MOD
};


#pragma pack(4)
static struct {
    bool acc_data_res;
    bool comp_data_res;
    u16 rsvd;
} result_struct;




/* ��������� ������������  */
#pragma section("FLASH_code")
bool lsm303_init_acc(void)
{
    u8 b;
    u8 axis = 0x27;		/*  X+Y+Z enabled, normal mode 10 Hz */
    u8 mode = 0x00;		/* Little endian, scale +- 2G, Low res */

    TWI_write_pack(LSM303_ACC_ADDR, CTRL_REG1_A, &axis, 1, &lsm303_acc_val);
    b = TWI_read_byte(LSM303_ACC_ADDR, CTRL_REG1_A, &lsm303_acc_val);
    if (b != axis)
	return false;

    TWI_write_pack(LSM303_ACC_ADDR, CTRL_REG4_A, &mode, 1, &lsm303_acc_val);
    b = TWI_read_byte(LSM303_ACC_ADDR, CTRL_REG4_A, &lsm303_acc_val);
    if (b != mode)
	return false;


    return true;
}


/* ��������� ������ */
#pragma section("FLASH_code")
bool lsm303_init_comp(void)
{
    u8 b;
    u8 rate = 0x0C | 0x80;	/* ������� 7.5 ��, + ������������� ������ ON */
    u8 gain = 0x20;		/* gn2=0 gn1=0 gn0=1 */
    u8 mode = 0x00;		/* Continiuos mode - ���������, ��� ��� ���������� ����� */
    bool res = false;

    do {
	TWI_write_pack(LSM303_COMP_ADDR, CRA_REG_M, &rate, 1, &lsm303_comp_val);
	b = TWI_read_byte(LSM303_COMP_ADDR, CRA_REG_M, &lsm303_acc_val);
	if (b != rate)
	    break;


	TWI_write_pack(LSM303_COMP_ADDR, CRB_REG_M, &gain, 1, &lsm303_comp_val);
	b = TWI_read_byte(LSM303_COMP_ADDR, CRB_REG_M, &lsm303_acc_val);
	if (b != gain)
	    break;

	TWI_write_pack(LSM303_COMP_ADDR, MR_REG_M, &mode, 1, &lsm303_comp_val);
	b = TWI_read_byte(LSM303_COMP_ADDR, MR_REG_M, &lsm303_acc_val);
	if (b != mode)
	    break;

	res = true;
    } while (0);


    return res;
}


/* �������� ������ ������������� */
#pragma section("FLASH_code")
bool lsm303_get_acc_data(lsm303_data * acc)
{
    u8 lo, hi, status;
    s16 x, y, z, t;
    bool res = false;
    int t0;

    /* ��������� ������������ */
    if (!(result_struct.acc_data_res)) {
	res = lsm303_init_acc();

	if (true == res)
	    result_struct.acc_data_res = true;
	else
	    return res;
    }

     t0 = get_msec_ticks();


    /* ���� ������� � ������� 100 �� - �� ����� �������. ����� ����� ��������� �������� */
    do {
	status = TWI_read_byte(LSM303_ACC_ADDR, STATUS_REG_A, &lsm303_acc_val);
	if (get_msec_ticks() - t0 < 100) {
	    return false;
//	    break;
	}
    } while (!(status & 8));


    /* ������ ��������  */
    lo = TWI_read_byte(LSM303_ACC_ADDR, OUT_X_L_A, &lsm303_acc_val);	/* �������  */
    hi = TWI_read_byte(LSM303_ACC_ADDR, OUT_X_H_A, &lsm303_acc_val);	/* �������  */
    x = ((u16) hi << 8) + lo;	/* ��������� endian */
    x /= 16;

    lo = TWI_read_byte(LSM303_ACC_ADDR, OUT_Y_L_A, &lsm303_acc_val);	/* �������  */
    hi = TWI_read_byte(LSM303_ACC_ADDR, OUT_Y_H_A, &lsm303_acc_val);	/* �������  */
    y = ((u16) hi << 8) + lo;	/* ��������� endian */
    y /= 16;

    lo = TWI_read_byte(LSM303_ACC_ADDR, OUT_Z_L_A, &lsm303_acc_val);	/* �������  */
    hi = TWI_read_byte(LSM303_ACC_ADDR, OUT_Z_H_A, &lsm303_acc_val);	/* �������  */
    z = ((u16) hi << 8) + lo;	/* ��������� endian */
    z /= 16;

    /* ������� ��������? */
    if (!(x == 0 && y == 0 && z == 0)) {
	acc->x = x;
	acc->y = y;
	acc->z = z;
	res = true;
    } else {
	acc->x = 0;
	acc->y = 0;
	acc->z = 0;
	res = false;
    }

    return res;
}

/* �������� ������ ������� */
#pragma section("FLASH_code")
bool lsm303_get_comp_data(lsm303_data * comp)
{
    bool res;
    u8 hi, lo, status;
    s16 x, y, z, t;
    int temp, t0;

    /* ��������� ������ */
    if (!(result_struct.comp_data_res)) {
	res = lsm303_init_comp();

	if (true == res)
	    result_struct.comp_data_res = true;
	else
	    return res;
    }

    /* ���� ������� � ������� 100 �� - �� ����� �������. ����� ����� ��������� �������� */
   t0 = get_msec_ticks();
    do {
	status = TWI_read_byte(LSM303_COMP_ADDR, SR_REG_M, &lsm303_acc_val);
	if (get_msec_ticks() - t0 < 100) {
	    return false;
//	    break;
	}
    } while (!(status & 1));


    hi = TWI_read_byte(LSM303_COMP_ADDR, OUT_X_H_M, &lsm303_comp_val);	/* �������  */
    lo = TWI_read_byte(LSM303_COMP_ADDR, OUT_X_L_M, &lsm303_comp_val);	/* �������  */
    x = ((u16) hi << 8) + lo;	/* ��������� endian */
    x /= 16;

    hi = TWI_read_byte(LSM303_COMP_ADDR, OUT_Z_H_M, &lsm303_comp_val);	/* �������  */
    lo = TWI_read_byte(LSM303_COMP_ADDR, OUT_Z_L_M, &lsm303_comp_val);	/* �������  */
    z = ((u16) hi << 8) + lo;	/* ��������� endian */
    z /= 16;

    hi = TWI_read_byte(LSM303_COMP_ADDR, OUT_Y_H_M, &lsm303_comp_val);	/* �������  */
    lo = TWI_read_byte(LSM303_COMP_ADDR, OUT_Y_L_M, &lsm303_comp_val);	/* �������  */
    y = ((u16) hi << 8) + lo;	/* ��������� endian */
    y /= 16;

    /* �������� ����������� - ��� ��������� � �������  */
    hi = TWI_read_byte(LSM303_COMP_ADDR, TEMP_OUT_H_M, &lsm303_comp_val);	/* �������  */
    lo = TWI_read_byte(LSM303_COMP_ADDR, TEMP_OUT_L_M, &lsm303_comp_val);

    temp = (s16) (((u16) hi << 8) | lo);	/* ��������� endian */
    temp *= 10;
    temp >>= 7;
    t = temp + 220;

    /* ������� ��������? */
    if (!(x == 0 && y == 0 && z == 0 && t == 220)) {
	comp->x = x;
	comp->y = y;
	comp->z = z;
	comp->t = t;
	res = true;
    } else {
	comp->x = 0;
	comp->y = 0;
	comp->z = 0;
	comp->t = 0;
	res = false;
    }

    return res;
}
