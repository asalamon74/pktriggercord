/*
    pkTriggerCord
    Copyright (C) 2011-2013 Andras Salamon <andras.salamon@melda.info>
    Remote control of Pentax DSLR cameras.

    based on:

    PK-Remote
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2008 Pontus Lidman <pontus@lysator.liu.se>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PSLR_SCSI_H
#define PSLR_SCSI_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

extern bool debug;
extern void write_debug( const char* message, ... );

#define DPRINT(x...) write_debug(x)

typedef enum {
    PSLR_OK = 0,
    PSLR_DEVICE_ERROR,
    PSLR_SCSI_ERROR,
    PSLR_COMMAND_ERROR,
    PSLR_READ_ERROR,
    PSLR_NO_MEMORY,
    PSLR_PARAM,                 /* Invalid parameters to API */
    PSLR_ERROR_MAX
} pslr_result;

int scsi_read(int sg_fd, uint8_t *cmd, uint32_t cmdLen,
		     uint8_t *buf, uint32_t bufLen);

int scsi_write(int sg_fd, uint8_t *cmd, uint32_t cmdLen,
               uint8_t *buf, uint32_t bufLen);

char **get_drives(int *driveNum);

pslr_result get_drive_info(char* driveName, int* hDevice, 
                            char* vendorId, int vendorIdSizeMax,
                            char* productId, int productIdSizeMax);

void close_drive(int *hDevice);
#endif
