/**
  ******************************************************************************
  * @file    memory.c
  * @author  MCD Application Team
  * @version V4.1.0
  * @date    26-May-2017
  * @brief   Memory management layer
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics International N.V. 
  * All rights reserved.</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */


/* Includes ------------------------------------------------------------------*/

#include "memory.h"
#include "usb_scsi.h"
#include "usb_bot.h"
#include "usb_regs.h"
#include "usb_mem.h"
#include "usb_conf.h"
#include "hw_config.h"
#include "mass_mal.h"
#include "usb_lib.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
__IO uint32_t Block_Read_count = 0;
__IO uint32_t Block_offset;
__IO uint32_t Counter = 0;
uint32_t Data_Buffer[BULK_MAX_PACKET_SIZE * 8]; /* 2048 bytes*/
uint8_t TransferState = TXFR_IDLE;
/* Extern variables ----------------------------------------------------------*/
extern uint8_t Bulk_Data_Buff[BULK_MAX_PACKET_SIZE];  /* data buffer*/
extern uint16_t Data_Len;
extern uint8_t Bot_State;
extern Bulk_Only_CSW CSW;

/* Private function prototypes -----------------------------------------------*/
/* Extern function prototypes ------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
* Function Name  : Read_Memory
* Description    : Handle the Read operation from the microSD card.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void Read_Memory(uint8_t lun, uint32_t LBA, uint32_t BlockCount)
{
	static uint32_t currentLBA, totalDataToTransfer;
	if(TransferState == TXFR_IDLE)
	{
		currentLBA = LBA;
		totalDataToTransfer = BlockCount * Mass_Block_Size[lun];
		TransferState = TXFR_ONGOING;
	}

	if(TransferState == TXFR_ONGOING)
	{
		if(Block_Read_count == 0)
		{
			MAL_Read(lun, currentLBA, Data_Buffer, 1); //read 512 bytes
			currentLBA += 1;

			USB_SIL_Write(EP1_IN, (uint8_t*) Data_Buffer, BULK_MAX_PACKET_SIZE); //transfer 64 bytes

			Block_Read_count = Mass_Block_Size[lun] - BULK_MAX_PACKET_SIZE;
			Block_offset = BULK_MAX_PACKET_SIZE;
		}
		else
		{
			USB_SIL_Write(EP1_IN, (uint8_t*) Data_Buffer + Block_offset, BULK_MAX_PACKET_SIZE);

			Block_Read_count -= BULK_MAX_PACKET_SIZE;
			Block_offset += BULK_MAX_PACKET_SIZE;
		}
		SetEPTxCount(ENDP1, BULK_MAX_PACKET_SIZE);
		SetEPTxStatus(ENDP1, EP_TX_VALID);

		totalDataToTransfer -= BULK_MAX_PACKET_SIZE;
		CSW.dDataResidue -= BULK_MAX_PACKET_SIZE;
	}
	if (totalDataToTransfer == 0)
	{
		Block_Read_count = 0;
		Block_offset = 0;
		Bot_State = BOT_DATA_IN_LAST;
		TransferState = TXFR_IDLE;
	}
}

/*******************************************************************************
* Function Name  : Write_Memory
* Description    : Handle the Write operation to the microSD card.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void Write_Memory(uint8_t lun, uint32_t LBA, uint32_t BlockCount)
{
	static uint32_t currentLBA, totalDataToWrite;
	if (TransferState == TXFR_IDLE)
	{
		currentLBA = LBA;
		totalDataToWrite = BlockCount * Mass_Block_Size[lun];
		TransferState = TXFR_ONGOING;
	}

	if (TransferState == TXFR_ONGOING)
	{
		for(int i = 0; i < BULK_MAX_PACKET_SIZE; i++, Counter++)
		{
			*((uint8_t*) Data_Buffer + Counter) = Bulk_Data_Buff[i];
		}
		if((Counter % Mass_Block_Size[lun]) == 0)
		{
			Counter = 0;
			MAL_Write(lun, currentLBA, Data_Buffer, 1);
			currentLBA += 1;
			totalDataToWrite -= Mass_Block_Size[lun];
		}
		CSW.dDataResidue -= Data_Len;
		SetEPRxStatus(ENDP2, EP_RX_VALID); /* enable the next transaction*/
	}
	if ((totalDataToWrite == 0) || (Bot_State == BOT_CSW_Send))
	{
		Counter = 0;
		Set_CSW(CSW_CMD_PASSED, SEND_CSW_ENABLE);
		TransferState = TXFR_IDLE;
	}
}
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

