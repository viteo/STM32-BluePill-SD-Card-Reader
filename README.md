STM32 BluePill SD Card Reader
===
Most MM/SD Cards support connection via SPI [(How to Use MMC/SDC by ChaN)](http://elm-chan.org/docs/mmc/mmc_e.html). In this way it is possible to connect a SD Card to BluePill and use it as an external memory storage, but also present it via Mass Storage Class as an USB Card Reader. Bear in mind that the read/write speed is relatively very low.


Using on STM32 BluePill
---
In STM32CubeIDE

    File -> Open Project from File System... -> Directory -> Finish

Connect MMC/SDC to SPI pins

       |¯¯¯¯¯¯¯¯¯\_/¯¯¯¯¯\                  +--------------+
       |                  \                 |              |
       |                CS |-2-----------18-| PB0          |
       |              MOSI |-3-----------17-| PA7          |
       |               VCC |-4--------+3.3V-| 3.3V out     |
       |              SCLK |-5-----------15-| PA5          |
       |               GND |-6----------GND-| GND          |
       |              MISO |-7-----------16-| PA6          |
       |___________________|                |              |
                   SD Card                  |     USB      |
                                            +--------------+
                                             BluePill

- [fatfs](http://elm-chan.org/fsw/ff/00index_e.html) lib by ChaN: R/W access to MMC from the BluePill side
- mmc lib by Internet: low level access to MMC in SPI Mode
- [usb-fs](https://www.st.com/en/embedded-software/stsw-stm32121.html) lib with MSC: R/W access to MMC from PC side via BluePill's USB


### The .ioc file
The .ioc file is just for reference. Do not use 'Generate Code'.