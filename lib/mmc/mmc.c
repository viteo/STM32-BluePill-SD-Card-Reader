#include "mmc.h"
#include "device.h"
#include "by_spi.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_gpio.h"

/********************************************************************/

/* Definitions for MMC/SDC command */
#define CMD0     (0x40+0)     /* GO_IDLE_STATE */
#define CMD1     (0x40+1)     /* SEND_OP_COND (MMC) */
#define CMD8     (0x40+8)     /* SEND_IF_COND */
#define CMD9     (0x40+9)     /* SEND_CSD */
#define CMD10    (0x40+10)    /* SEND_CID */
#define CMD12    (0x40+12)    /* STOP_TRANSMISSION */
#define ACMD13	 (0x80+13)    /* SD_STATUS (SDC) */
#define CMD16    (0x40+16)    /* SET_BLOCKLEN */
#define CMD17    (0x40+17)    /* READ_SINGLE_BLOCK */
#define CMD18    (0x40+18)    /* READ_MULTIPLE_BLOCK */
#define CMD23    (0x40+23)    /* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	 (0x80+23)    /* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24    (0x40+24)    /* WRITE_BLOCK */
#define CMD25    (0x40+25)    /* WRITE_MULTIPLE_BLOCK */
#define CMD32	 (0x40+32)    /* ERASE_ER_BLK_START */
#define CMD33	 (0x40+33)    /* ERASE_ER_BLK_END */
#define CMD38	 (0x40+38)    /* ERASE */
#define CMD41    (0x40+41)    /* SEND_OP_COND (ACMD) */
#define	ACMD41   (0x80+41)	  /* SEND_OP_COND (SDC) */
#define CMD55    (0x40+55)    /* APP_CMD */
#define CMD58    (0x40+58)    /* READ_OCR */

/* MMC card type flags (MMC_GET_TYPE) */
#define CT_MMC		0x01		/* MMC ver 3 */
#define CT_SD1		0x02		/* SD ver 1 */
#define CT_SD2		0x04		/* SD ver 2 */
#define CT_SDC		(CT_SD1|CT_SD2)	/* SD */
#define CT_BLOCK	0x08		/* Block addressing */

volatile uint8_t SDTimer1, SDTimer2;                    /* 10ms Timer Counter */
static volatile DSTATUS Stat = STA_NOINIT;				/* Disk Status */
static uint8_t CardType;                                /* Type 0:MMC, 1:SDC, 2:Block addressing */
static uint8_t PowerFlag = 0;                           /* Power flag */

/* SPI Chip Select */
static void SPI_ChipSelect(void)
{
	GPIO_WriteBit(GPIOB, PIN_SPI_CS, Bit_RESET);
}

/* SPI Chip Deselect */
static void SPI_ChipDeselect(void)
{
	GPIO_WriteBit(GPIOB, PIN_SPI_CS, Bit_SET);
}

/* SPI transmit a byte */
static void SPI_TxByte(BYTE data)
{
	while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
	SPI_I2S_SendData(SPI1, data);
	while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) != RESET);
	__IO uint32_t tmpreg_ovr = 0x00U;
	tmpreg_ovr = SPI1->DR;
	tmpreg_ovr = SPI1->SR;
	(void) (tmpreg_ovr);
}

/* SPI receive a byte */
static uint8_t SPI_RxByte(void)
{
	uint8_t dummy, data;
	dummy = 0xFF;
	data = 0;

	while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
	SPI_I2S_SendData(SPI1, dummy);
	while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
	data = SPI_I2S_ReceiveData(SPI1);
	while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) != RESET);
	__IO uint32_t tmpreg_ovr = 0x00U;
	tmpreg_ovr = SPI1->DR;
	tmpreg_ovr = SPI1->SR;
	(void) (tmpreg_ovr);

	return data;
}

/* write received byte */
static void SPI_RxBytePtr(uint8_t *buff)
{
	*buff = SPI_RxByte();
}

void SD_SysTickTimers()
{
	if (SDTimer1 > 0)
		SDTimer1--;

	if (SDTimer2 > 0)
		SDTimer2--;
}

/* power on */
static void SD_PowerOn(void)
{
	uint8_t cmd_arg[6];
	uint32_t Count = 0x1FFF;

	/* transmit bytes to wake up */
	SPI_ChipDeselect();

	for (int i = 0; i < 10; i++)
	{
		SPI_TxByte(0xFF);
	}

	/* SPI Chip Select */
	SPI_ChipSelect();

	/* make idle state */
	cmd_arg[0] = CMD0; /* CMD0:GO_IDLE_STATE */
	cmd_arg[1] = 0;
	cmd_arg[2] = 0;
	cmd_arg[3] = 0;
	cmd_arg[4] = 0;
	cmd_arg[5] = 0x95;/* CRC */

	/* send cmd_arg */
	for (int i = 0; i < 6; i++)
	{
		SPI_TxByte(cmd_arg[i]);
	}

	/* wait response */
	while ((SPI_RxByte() != 0x01) && Count)
	{
		Count--;
	}

	SPI_ChipDeselect();
	SPI_TxByte(0XFF);

	PowerFlag = 1;
}

static void SD_PowerOff(void)
{
	PowerFlag = 0;
}

/* wait SD ready */
static uint8_t SD_ReadyWait(void)
{
	uint8_t res;

	/* timeout 500ms */
	SDTimer2 = 50;
	SPI_RxByte();

	do
	{
		/* if SD goes ready, receives 0xFF */
		res = SPI_RxByte();
	} while ((res != 0xFF) && SDTimer2);

	return res;
}

/* transmit command */
static BYTE SD_SendCmd(BYTE cmd, DWORD arg)
{
	uint8_t crc, res;

	 /* Send a CMD55 prior to ACMD<n> */
	if (cmd & 0x80)
	{
		cmd &= 0x7F;
		res = SD_SendCmd(CMD55, 0);
		if (res > 1)
			return res;
	}

	/* wait SD ready */
	if (SD_ReadyWait() != 0xFF)
		return 0xFF;

	/* transmit command */
	SPI_TxByte(cmd);				/* Command */
	SPI_TxByte((BYTE) (arg >> 24));	/* Argument[31..24] */
	SPI_TxByte((BYTE) (arg >> 16));	/* Argument[23..16] */
	SPI_TxByte((BYTE) (arg >> 8));	/* Argument[15..8] */
	SPI_TxByte((BYTE) arg);			/* Argument[7..0] */

	/* prepare CRC */
	crc = 0;
	if (cmd == CMD0)
		crc = 0x95; /* CRC for CMD0(0) */

	if (cmd == CMD8)
		crc = 0x87; /* CRC for CMD8(0x1AA) */

	/* transmit CRC */
	SPI_TxByte(crc);

	/* Skip a stuff byte when STOP_TRANSMISSION */
	if (cmd == CMD12)
		SPI_RxByte();

	/* Wait for response (10 bytes max) */
	uint8_t n = 10;
	do
	{
		res = SPI_RxByte();
	} while ((res & 0x80) && --n);

	return res;
}

/* transmit data block */
#if _READONLY == 0
static uint8_t SD_TxDataBlock(const BYTE *buff, BYTE token)
{
	uint8_t resp;

	/* wait SD ready */
	if (SD_ReadyWait() != 0xFF)
		return 0;

	/* transmit token */
	SPI_TxByte(token);

	/* if it's not STOP token, transmit data */
	if (token != 0xFD)
	{
		/* 512 bytes of data transfer */
		for(int i = 0; i < 512; i++)
		{
			SPI_TxByte(*buff++);
		}

		/* discard CRC */
		SPI_RxByte();
		SPI_RxByte();

		/* receive response */
		int i = 0;
		while (i <= 64)
		{
			resp = SPI_RxByte();

			/* transmit 0x05 accepted */
			if ((resp & 0x1F) == 0x05)
				break;

			i++;
		}

		/* recv buffer clear */
		while (SPI_RxByte() == 0);
	}

	/* transmit 0x05 accepted */
	if ((resp & 0x1F) == 0x05)
		return 1;
	return 0;
}
#endif /* _READONLY */

/* receive data block */
/* buff: Data buffer */
/* btr: Data block length (byte) */
static uint8_t SD_RxDataBlock(BYTE *buff, UINT btr)
{
	uint8_t token;

	/* timeout 100ms */
	SDTimer1 = 10;

	/* loop until receive a response or timeout */
	do
	{
		token = SPI_RxByte();
	} while ((token == 0xFF) && SDTimer1);

	/* invalid response */
	if (token != 0xFE)
		return 0;

	/* receive data */
	do
	{
		SPI_RxBytePtr(buff++);
		SPI_RxBytePtr(buff++);
	} while (btr -= 2);

	/* discard CRC */
	SPI_RxByte();
	SPI_RxByte();

	return 1;
}

DSTATUS MMC_disk_initialize()
{
	uint8_t n, type, ocr[4];

	/* no disk */
	if (Stat & STA_NODISK)
		return Stat;

	/* power on */
	SD_PowerOn();

	/* slave select */
	SPI_ChipSelect();

	/* check disk type */
	type = 0;

	/* send GO_IDLE_STATE command */
	if (SD_SendCmd(CMD0, 0) == 1)
	{
		/* timeout 1 sec */
		SDTimer1 = 100;

		/* check SDC V2+: accept CMD8 command */
		if (SD_SendCmd(CMD8, 0x1AA) == 1)
		{
			/* operation condition register */
			for (n = 0; n < 4; n++)
			{
				ocr[n] = SPI_RxByte();
			}

			/* Is the card supports vcc of 2.7-3.6V? */
			if (ocr[2] == 0x01 && ocr[3] == 0xAA)
			{
				/* ACMD41 with HCS bit */
				do
				{
					/* Wait for end of initialization with ACMD41(HCS) */
					//if( SD_SendCmd(ACMD41, 1UL << 30))
					if (SD_SendCmd(CMD55, 0) <= 1 && SD_SendCmd(CMD41, 1UL << 30) == 0)
						break;
				} while (SDTimer1);

				/* READ_OCR */
				if (SDTimer1 && SD_SendCmd(CMD58, 0) == 0)
				{
					/* Check CCS bit in the OCR */
					for (n = 0; n < 4; n++)
					{
						ocr[n] = SPI_RxByte();
					}
					/* Card id SDv2 (HC or SC) */
					type = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;
				}
			}
		}
		else
		{
			/* SDC Ver1 or MMC */
			/*if (SD_SendCmd(ACMD41, 0) <= 1)
				type = CT_SD1; // SDv1 (ACMD41(0))
			else
				type = CT_MMC; // MMCv3 (CMD1(0))
			*/
			type = (SD_SendCmd(CMD55, 0) <= 1 && SD_SendCmd(CMD41, 0) <= 1) ? CT_SD1 : CT_MMC; /* SDC : MMC */

			do
			{
				if (type == CT_SD1)
				{
					if (SD_SendCmd(CMD55, 0) <= 1 && SD_SendCmd(CMD41, 0) == 0)
						break; /* ACMD41 */
				}
				else
				{
					if (SD_SendCmd(CMD1, 0) == 0)
						break; /* CMD1 */
				}
			} while (SDTimer1);

			/* Set block length: 512 */
			if (!SDTimer1 || SD_SendCmd(CMD16, 512) != 0)
			{
				/* SET_BLOCKLEN */
				type = 0;
			}
		}
	}

	CardType = type;

	SPI_ChipDeselect();

	SPI_RxByte(); /* Idle state (Release DO) */

	if (type)
	{
		/* Clear STA_NOINIT */
		Stat &= ~STA_NOINIT;
	}
	else
	{
		/* Initialization failed */
		SD_PowerOff();
	}

	return Stat;
}

DSTATUS MMC_disk_status()
{
	return Stat;
}

DRESULT MMC_disk_read(uint8_t *buff, uint32_t sector, uint32_t count)
{
	if (!count)
		return RES_PARERR;

	/* no disk */
	if (Stat & STA_NOINIT)
		return RES_NOTRDY;

	/* convert sector to byte address */
	if (!(CardType & CT_BLOCK))
		sector *= 512;

	SPI_ChipSelect();

	if (count == 1)
	{
		/* READ_SINGLE_BLOCK */
		if ((SD_SendCmd(CMD17, sector) == 0) && SD_RxDataBlock(buff, 512))
			count = 0;
	}
	else
	{
		/* READ_MULTIPLE_BLOCK */
		if (SD_SendCmd(CMD18, sector) == 0)
		{
			do
			{
				if (!SD_RxDataBlock(buff, 512))
					break;

				buff += 512;
			} while (--count);

			/* Request to stop transmission after reading all blocks */
			SD_SendCmd(CMD12, 0);
		}
	}

	/* Idle (Release DO) */
	SPI_ChipDeselect();
	SPI_RxByte();

	return count ? RES_ERROR : RES_OK;

}

#if FF_FS_READONLY == 0
DRESULT MMC_disk_write(const uint8_t *buff, uint32_t sector, uint32_t count)
{
	/* Check drive status */
	if (Stat & STA_NOINIT)
		return RES_NOTRDY;

	/* Check write protection */
	if (Stat & STA_PROTECT)
		return RES_WRPRT;

	/* Convert sector to byte address */
	if (!(CardType & CT_BLOCK))
		sector *= 512;

	SPI_ChipSelect();

	if (count == 1)
	{
		/* WRITE_BLOCK */
		if ((SD_SendCmd(CMD24, sector) == 0) && SD_TxDataBlock(buff, 0xFE))
			count = 0;
	}
	else
	{
		/* WRITE_MULTIPLE_BLOCK */
		if (CardType & CT_SD1)
		{
			SD_SendCmd(CMD55, 0);
			SD_SendCmd(CMD23, count); /* ACMD23 */
		}
		/*if (CardType & CT_SDC)
			SD_SendCmd(ACMD23, count);*/

		if (SD_SendCmd(CMD25, sector) == 0)
		{
			do
			{
				if (!SD_TxDataBlock(buff, 0xFC))
					break;

				buff += 512;
			} while (--count);

			/* STOP_TRAN token */
			if (!SD_TxDataBlock(0, 0xFD))
			{
				count = 1;
			}
		}
	}

	/* Idle */
	SPI_ChipDeselect();
	SPI_RxByte();

	return count ? RES_ERROR : RES_OK;
}
#endif


DRESULT MMC_disk_ioctl(uint8_t cmd, void *buff)
{
	DRESULT res;
	BYTE n, csd[16], *ptr = buff;
	DWORD *dp, st, ed;
	WORD csize;

	res = RES_ERROR;

	if (cmd == CTRL_POWER)
	{
		switch (*ptr)
		{
		case 0:
			if (PowerFlag)
				SD_PowerOff(); /* Power Off */
			res = RES_OK;
			break;
		case 1:
			SD_PowerOn(); /* Power On */
			res = RES_OK;
			break;
		case 2:
			*(ptr + 1) = (BYTE) PowerFlag;
			res = RES_OK; /* Power Check */
			break;
		default:
			res = RES_PARERR;
		}
	}
	else
	{
		if (Stat & STA_NOINIT)
			return RES_NOTRDY;

		SPI_ChipSelect();

		switch (cmd)
		{
		case GET_SECTOR_COUNT:
			if ((SD_SendCmd(CMD9, 0) == 0) && SD_RxDataBlock(csd, 16))
			{
				if ((csd[0] >> 6) == 1)
				{
					/* SDC V2 */
					csize = csd[9] + ((WORD) csd[8] << 8) + 1;
					*(DWORD*) buff = (DWORD) csize << 10;
				}
				else
				{
					/* MMC or SDC V1 */
					n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
					csize = (csd[8] >> 6) + ((WORD) csd[7] << 2) + ((WORD) (csd[6] & 3) << 10) + 1;
					*(DWORD*) buff = (DWORD) csize << (n - 9);
				}

				res = RES_OK;
			}
			break;

		case GET_SECTOR_SIZE:
			*(WORD*) buff = 512;
			res = RES_OK;
			break;

		case CTRL_SYNC:
			if (SD_ReadyWait() == 0xFF)
				res = RES_OK;
			break;

		case MMC_GET_CSD:
			if (SD_SendCmd(CMD9, 0) == 0 && SD_RxDataBlock(ptr, 16))
				res = RES_OK;
			break;

		case MMC_GET_CID:
			if (SD_SendCmd(CMD10, 0) == 0 && SD_RxDataBlock(ptr, 16))
				res = RES_OK;
			break;

		case MMC_GET_OCR:
			if (SD_SendCmd(CMD58, 0) == 0)
			{
				for (n = 0; n < 4; n++)
				{
					*ptr++ = SPI_RxByte();
				}

				res = RES_OK;
			}

		case CTRL_TRIM: /* Erase a block of sectors (used when FF_USE_TRIM == 1) */
			if (!(CardType & CT_SDC))
				break;
			/* Check if the card is SDC */
			/* Get CSD */
			if (MMC_disk_ioctl(MMC_GET_CSD, csd))
				break;
			/* Check if sector erase can be applied to the card */
			if (!(csd[0] >> 6) && !(csd[10] & 0x40))
				break;
			dp = buff;
			st = dp[0];
			ed = dp[1];
			/* Load sector block */
			if (!(CardType & CT_BLOCK))
			{
				st *= 512;
				ed *= 512;
			}
			/* Erase sector block */
			if (SD_SendCmd(CMD32, st) == 0 && SD_SendCmd(CMD33, ed) == 0 && SD_SendCmd(CMD38, 0) == 0 && SD_ReadyWait())
			{
				res = RES_OK; /* FatFs does not check result of this command */
			}
			break;

		default:
			res = RES_PARERR;
		}

		SPI_ChipDeselect();
		SPI_RxByte();
	}

	return res;
}

