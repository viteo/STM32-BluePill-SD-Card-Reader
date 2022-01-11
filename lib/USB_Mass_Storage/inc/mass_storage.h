/*
 * mass_storage.h
 *
 *  Created on: 15 Jan 2021
 *      Author: v.simonenko
 */

#ifndef INC_MASS_STORAGE_H_
#define INC_MASS_STORAGE_H_
#include <stdint.h>

void MSD_Init(void);
void MSD_Deinit(void);
uint8_t MSD_IsEjected();

#endif /* INC_MASS_STORAGE_H_ */
