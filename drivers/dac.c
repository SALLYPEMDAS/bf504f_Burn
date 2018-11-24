/* ������� ��� ���������� AD5620 - ��� ������������ ��� 
 * � ������ �������� ������� ����� �� ��������!!! */
#include "dac.h"
#include "xpander.h"
#include "utils.h"
#include "spi1.h"


/*************************************************************
 * � ��� ������������ ����������: 14-�� ������ AD5640 
 * ���������� �� ������: Vout = 2.5 * D / 16384 
 ************************************************************/

/* ���������������� ��� ����  */
#pragma section("FLASH_code")
void DAC_init(void)
{
	pin_set(DAC192MHZ_CS_PORT, DAC192MHZ_CS_PIN);	/* ������� 19.2 � �������. � ����������  */
#if QUARTZ_CLK_FREQ==(19200000)
	pin_set(DAC4MHZ_CS_PORT, DAC4MHZ_CS_PIN);	/* 4.096 � �������. � ����������  */
/*#elif QUARTZ_CLK_FREQ==(8192000)*/
/*	pin_set(DAC_TEST_CS_PORT, DAC_TEST_CS_PIN);*/	/* �������� � �������. � ����������  */
#endif

}


/* �������� ����� � ���, xpander � SPI1 ��� ������ ���� ��������� */
#pragma section("FLASH_code")
void DAC_write(DAC_TYPE_ENUM type, u16 cmd)
{
	u8 port, pin, dir;

	/* ����� � ��� DAC?  */
	switch (type) {

		/* ��� ������ 19.2 ��� */
	case DAC_19MHZ:
		port = DAC192MHZ_CS_PORT;
		pin = DAC192MHZ_CS_PIN;
		dir = DAC192MHZ_CS_DIR;
		break;

		/* �������� ��� */
	case DAC_TEST:
		port = DAC_TEST_CS_PORT;
		pin = DAC_TEST_CS_PIN;
		dir = DAC_TEST_CS_DIR;
		break;

		/* ��� ������ 4 ��� */
	case DAC_4MHZ:
	default:
		port = DAC4MHZ_CS_PORT;
		pin = DAC4MHZ_CS_PIN;
		dir = DAC4MHZ_CS_DIR;
		break;
	}

	/* ��������� CS ����� */
	pin_clr(port, pin);
	
	delay_ms(5);		/* �������� ����� ������� - CS ����� ��������� � �������� �������� */

	SPI1_write_read(cmd & 0x3fff);	/* CS ������ - �������� ������� �� DMA7 */
	pin_set(port, pin);		/* ������� CS �� ���������� 1 ����� */
}
