/*
 * mass_storage.c
 *
 *  Created on: 15 Jan. 2021
 *      Author: v.simonenko
 */

#include "mass_storage.h"
#include "hw_config.h"
#include "usb_init.h"
#include "generic.h"
#include "stm32f10x_gpio.h"

void MSD_Init(void)
{
  Set_System();
  Set_USBClock();
  USB_Interrupts_Config(ENABLE);
  USB_Init();

  //while (bDeviceState != CONFIGURED); //wait for cable being plugged and device configured
}

#include "stm32f10x_flash.h"
#include "usb_pwr.h"
#include "usb_regs.h"
void MSD_Deinit()
{
	USB_Interrupts_Config(DISABLE);
	PowerOff();
	FLASH_Lock();
	_SetCNTR(CNTR_PDWN | CNTR_FRES);

/********************************************/
/*  Configure USB DM/DP pins                */
/********************************************/
	//Digital line soft reset, so the PC feels like the cable has been unplugged.
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOA, GPIO_Pin_12);
	DWT_Delay_ms(5);

//	GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
}

uint8_t MSD_IsEjected()
{
	uint16_t wCNTR;
	wCNTR = _GetCNTR();
	if((wCNTR & CNTR_FSUSP) && (wCNTR & CNTR_LPMODE))
		return 1;
	return 0;
}
