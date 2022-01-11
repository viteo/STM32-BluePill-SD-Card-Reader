#ifndef MMC_H_
#define MMC_H_

#include "diskio.h"

void SD_SysTickTimers();
DSTATUS MMC_disk_initialize();
DSTATUS MMC_disk_status();
DRESULT MMC_disk_read(uint8_t *buff, uint32_t sector, uint32_t count);
DRESULT MMC_disk_write(const uint8_t *buff, uint32_t sector, uint32_t count);
DRESULT MMC_disk_ioctl(uint8_t cmd, void *buff);

#endif /* MMC_H_ */
