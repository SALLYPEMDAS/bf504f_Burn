/*******************************************************************************
 * � ���� ����� ����������� ������������� PLL ��� ����������� � ������� ������
 *******************************************************************************/
#include <ccblkfn.h>
#include <bfrom.h>
#include "config.h"
#include "main.h"
#include "pll.h"


/*  ������������� PLL, �� ������� � ����� ��������� */
#pragma section("FLASH_code")
void PLL_init(void)
{
	u32 SIC_IWR1_reg;	/* backup SIC_IWR1 register */

	/* use Blackfin ROM SysControl() to change the PLL */
	ADI_SYSCTRL_VALUES sysctrl = {
		PLLVRCTL_VALUE,   	/* ����������� �� ����������� �� ����� PF8 � PF9 */
		PLLCTL_VALUE,		/* MSEL[5:0] = 10 - �������� VCO = 192��� �� 19.2 */
		PLLDIV_VALUE,		/* CSEL[1:0] = 0  - �������� CCLK = VSO/ 1 = 192���, SSEL[3:0] = 6  - �������� SCLK = VSO/4 = 48��� */
		PLLLOCKCNT_VALUE,	/* ����� 512 ������ ������� */
		PLLSTAT_VALUE		/* NB: ������ ������!!!  */
	};

	SIC_IWR1_reg = *pSIC_IWR1;	/* save SIC_IWR1 due to anomaly 05-00-0432 */
	*pSIC_IWR1 = 0;		/* disable wakeups from SIC_IWR1 */

	/* use the ROM function */
	bfrom_SysControl(SYSCTRL_WRITE | SYSCTRL_PLLCTL | SYSCTRL_PLLDIV, &sysctrl, NULL);
	*pSIC_IWR1 = SIC_IWR1_reg;	/* restore SIC_IWR1 due to anomaly 05-00-0432 */
}



/* ��������� ��������� � ������ �����  */
#pragma section("L1_code")
void PLL_sleep(DEV_STATE_ENUM state)
{

     if(state != DEV_COMMAND_MODE_STATE && state != DEV_POWER_ON_STATE && state != DEV_CHOOSE_MODE_STATE) {
	ADI_SYSCTRL_VALUES sleep;

	/* ��������� */
	bfrom_SysControl(SYSCTRL_EXTVOLTAGE | SYSCTRL_PLLCTL | SYSCTRL_READ, &sleep, NULL);
        sleep.uwPllCtl |= STOPCK;       /* ������� �� Sleep ����� */

         /* �������, ������� ����� ��� ������ */
	*pSIC_IWR0 = IRQ_UART0_ERR | IRQ_UART1_ERR | IRQ_PFA_PORTF;

#if QUARTZ_CLK_FREQ==(19200000)
	*pSIC_IWR1 = IRQ_TIMER0 | IRQ_TIMER1 | IRQ_TIMER2 | IRQ_TIMER3 | IRQ_TIMER4 | IRQ_TIMER5 | IRQ_PFA_PORTG;
#else
        /* ��� 3-�� ������� */
	*pSIC_IWR1 = IRQ_TIMER0 | IRQ_TIMER1 | IRQ_TIMER2 | IRQ_TIMER4 | IRQ_TIMER5 | IRQ_PFA_PORTG;
#endif

	/* � �������� - ��� ����� �������� */
	bfrom_SysControl(SYSCTRL_WRITE | SYSCTRL_EXTVOLTAGE | SYSCTRL_PLLCTL, &sleep, NULL);
    }
}

/* ��������� ��������� � ������� ����� */
#pragma section("L1_code")
void PLL_fullon(void)
{
	ADI_SYSCTRL_VALUES fullon;

	/* ��������� */
	bfrom_SysControl (SYSCTRL_READ | SYSCTRL_EXTVOLTAGE | SYSCTRL_PLLCTL, &fullon, NULL);
        fullon.uwPllCtl &= ~STOPCK; 

	/* � ��������, ��� ����� �������� ������������ - ��� ����� �������� */
	bfrom_SysControl(SYSCTRL_WRITE | SYSCTRL_EXTVOLTAGE | SYSCTRL_PLLCTL, &fullon, NULL);
}


/* ���������� */
#pragma section("L1_code")
void PLL_hibernate(DEV_STATE_ENUM v)
{
	ADI_SYSCTRL_VALUES hibernate;

	hibernate.uwVrCtl = 
	    WAKE_EN0 |		/* PH0 Wake-Up Enable */
	    WAKE_EN1 |		/* PF8 Wake-Up Enable */
	    WAKE_EN2 |		/* PF9 Wake-Up Enable */
	    CANWE |		/* CAN Rx Wake-Up Enable */
	    HIBERNATE;
	bfrom_SysControl(SYSCTRL_WRITE | SYSCTRL_VRCTL | SYSCTRL_EXTVOLTAGE| SYSCTRL_HIBERNATE , &hibernate, NULL);
}


/* �������� ��������� */
#pragma section("L1_code")
void PLL_reset(void)
{
	bfrom_SysControl(SYSCTRL_SYSRESET, NULL, NULL); /* either */
	bfrom_SysControl(SYSCTRL_SOFTRESET, NULL, NULL); /* or */
}

