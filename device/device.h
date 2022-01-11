/*
 * device.h
 *
 * Device/project description
 *
 */

#ifndef DEVICE_DEVICE_H_
#define DEVICE_DEVICE_H_
#include "stm32f10x_gpio.h"

//GPIO Defines
#define PIN_LED GPIO_Pin_13		//PC13	BluePill Green LED

#define PIN_SPI_CS GPIO_Pin_0	//PB0	SPI SD ChipSelect
#define PIN_SPI_SCK GPIO_Pin_5	//PA1	SPI Clock
#define PIN_SPI_MISO GPIO_Pin_6	//PA1	SPI Master Input
#define PIN_SPI_MOSI GPIO_Pin_7	//PA1	SPI Master Output

#define PIN_USB_DM GPIO_Pin_11	//PA11	USB Data-
#define PIN_USB_DP GPIO_Pin_12	//PA12	USB Data+

#endif /* DEVICE_DEVICE_H_ */
