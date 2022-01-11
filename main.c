#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "generic.h"
#include "device.h"
#include "mass_storage.h"
#include "fatfs.h"
#include "by_spi.h"
#include <stdlib.h>

#define DEFAULT_DELAY 100
#define LEN 20

int main()
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = PIN_LED;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	GPIO_InitStructure.GPIO_Pin = PIN_SPI_CS;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	DWT_Delay_Init();

	RCC_ClocksTypeDef RCC_Clocks;
	RCC_GetClocksFreq(&RCC_Clocks);
	SysTick_Config(RCC_Clocks.HCLK_Frequency / 1000);

	SPI1_Init();
	FatFs_Init();
	MSD_Init();

	char line[LEN] = {0};
//	const char* filename = "file1.txt";
//	file_open(filename);
//	file_readline(line, LEN);
//	file_close();
	uint32_t delay = atoi(line);
	delay = delay ? delay : DEFAULT_DELAY;

	while (1)
	{
		GPIO_ToggleBits(GPIOC, PIN_LED);
		DWT_Delay_ms(delay);
	}
}
