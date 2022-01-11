/*
 * fatfs.h
 *
 *  Created on: Jan 15, 2021
 *      Author: v.simonenko
 */

#ifndef FATFS_FATFS_H_
#define FATFS_FATFS_H_
#include <stdint.h>

uint8_t FatFs_Init();
uint8_t file_open(const char *path);
uint8_t file_readline(char *line_buffer, uint8_t buffer_size);
uint8_t file_close();

#endif /* FATFS_FATFS_H_ */
