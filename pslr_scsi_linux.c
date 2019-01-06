/*
    pkTriggerCord
    Copyright (C) 2011-2019 Andras Salamon <andras.salamon@melda.info>
    Remote control of Pentax DSLR cameras.

    based on:

    PK-Remote
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2008 Pontus Lidman <pontus@lysator.liu.se>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU General Public License
    and GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#ifdef ANDROID
#include "android_scsi_sg.h"
#else
#include <scsi/sg.h>
#endif
#include <unistd.h>
#include <dirent.h>
#include "pslr_model.h"

#include "pslr_scsi.h"

void print_scsi_error(sg_io_hdr_t *pIo, uint8_t *sense_buffer) {
    int k;

    if (pIo->sb_len_wr > 0) {
        DPRINT("SCSI error: sense data: ");
        for (k = 0; k < pIo->sb_len_wr; ++k) {
            if ((k > 0) && (0 == (k % 10))) {
                DPRINT("\n  ");
            }
            DPRINT("0x%02x ", sense_buffer[k]);
        }
        DPRINT("\n");
    }
    if (pIo->masked_status) {
        DPRINT("SCSI status=0x%x\n", pIo->status);
    }
    if (pIo->host_status) {
        DPRINT("host_status=0x%x\n", pIo->host_status);
    }
    if (pIo->driver_status) {
        DPRINT("driver_status=0x%x\n", pIo->driver_status);
    }
}

const char* device_dirs[2] = {"/sys/class/scsi_generic", "/sys/block"};

char **get_drives(int *driveNum) {
    DIR *d;
    struct dirent *ent;
    char *tmp[256];
    char **ret=NULL;
    int j=0,jj;
    int di;
    int device_num = sizeof(device_dirs)/sizeof(device_dirs[0]);
    for (di=0; di<device_num; ++di) {
        d = opendir(device_dirs[di]);
        if (d) {
            while ( (ent = readdir(d)) ) {
                if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
                    tmp[j] = malloc( strlen(ent->d_name)+1 );
                    strncpy(tmp[j], ent->d_name, strlen(ent->d_name)+1);
                    ++j;
                }
            }
            closedir(d);
        } else {
            DPRINT("Cannot open %s\n", device_dirs[di]);
        }
    }
    *driveNum = j;
    if (j>0) {
        ret = malloc( j * sizeof(char*) );
        for ( jj=0; jj<j; ++jj ) {
            ret[jj] = malloc( strlen(tmp[jj])+1 );
            strncpy( ret[jj], tmp[jj], strlen(tmp[jj]) );
            ret[jj][strlen(tmp[jj])]='\0';
        }
    }
    return ret;
}

pslr_result get_drive_info(char* driveName, int* hDevice,
                           char* vendorId, int vendorIdSizeMax,
                           char* productId, int productIdSizeMax) {
    char nmbuf[256];
    int fd;

    vendorId[0] = '\0';
    productId[0] = '\0';
    snprintf(nmbuf, sizeof (nmbuf), "/sys/class/scsi_generic/%s/device/vendor", driveName);
    fd = open(nmbuf, O_RDONLY);
    if (fd == -1) {
        snprintf(nmbuf, sizeof (nmbuf), "/sys/block/%s/device/vendor", driveName);
        fd = open(nmbuf, O_RDONLY);
        if (fd == -1) {
            return PSLR_DEVICE_ERROR;
        }
    }
    int v_length = read(fd, vendorId, vendorIdSizeMax-1);
    vendorId[v_length]='\0';
    close(fd);

    snprintf(nmbuf, sizeof (nmbuf), "/sys/class/scsi_generic/%s/device/model", driveName);
    fd = open(nmbuf, O_RDONLY);
    if (fd == -1) {
        snprintf(nmbuf, sizeof (nmbuf), "/sys/block/%s/device/model", driveName);
        fd = open(nmbuf, O_RDONLY);
        if (fd == -1) {
            return PSLR_DEVICE_ERROR;
        }
    }
    int p_length = read(fd, productId, productIdSizeMax-1);
    productId[p_length]='\0';
    close(fd);

    snprintf(nmbuf, sizeof (nmbuf), "/dev/%s", driveName);
    *hDevice = open(nmbuf, O_RDWR);
    if ( *hDevice == -1) {
        snprintf(nmbuf, sizeof (nmbuf), "/dev/block/%s", driveName);
        *hDevice = open(nmbuf, O_RDWR);
        if ( *hDevice == -1 ) {
            return PSLR_DEVICE_ERROR;
        }
    }
    return PSLR_OK;
}

void close_drive(int *hDevice) {
    close( *hDevice );
}

int scsi_read(int sg_fd, uint8_t *cmd, uint32_t cmdLen,
              uint8_t *buf, uint32_t bufLen) {
    sg_io_hdr_t io;
    uint8_t sense[32];
    int r;
    uint32_t i;

    memset(&io, 0, sizeof (io));

    io.interface_id = 'S';
    io.cmd_len = cmdLen;
    /* io.iovec_count = 0; */ /* memset takes care of this */
    io.mx_sb_len = sizeof (sense);
    io.dxfer_direction = SG_DXFER_FROM_DEV;
    io.dxfer_len = bufLen;
    io.dxferp = buf;
    io.cmdp = cmd;
    io.sbp = sense;
    io.timeout = 20000; /* 20000 millisecs == 20 seconds */
    /* io.flags = 0; */ /* take defaults: indirect IO, etc */
    /* io.pack_id = 0; */
    /* io.usr_ptr = NULL; */

    DPRINT("[S]\t\t\t\t\t >>> [");
    for (i = 0; i < cmdLen; ++i) {
        if (i > 0) {
            DPRINT(" ");
            if ((i%4) == 0 ) {
                DPRINT(" ");
            }
        }
        DPRINT("%02X", cmd[i]);
    }
    DPRINT("]\n");

    r = ioctl(sg_fd, SG_IO, &io);
    if (r == -1) {
        perror("ioctl");
        return -PSLR_DEVICE_ERROR;
    }

    if ((io.info & SG_INFO_OK_MASK) != SG_INFO_OK) {
        print_scsi_error(&io, sense);
        return -PSLR_SCSI_ERROR;
    } else {
        DPRINT("[S]\t\t\t\t\t <<< [");
        for (i = 0; i < 32 && i < (bufLen - io.resid); ++i) {
            if (i > 0) {
                DPRINT(" ");
                if (i % 16 == 0) {
                    DPRINT("\n\t\t\t\t\t      ");
                } else if ((i%4) == 0 ) {
                    DPRINT(" ");
                }
            }
            DPRINT("%02X", buf[i]);
        }
        DPRINT("]\n");

        /* Older Pentax DSLR will report all bytes remaining, so make
         * a special case for this (treat it as all bytes read). */
        if (io.resid == bufLen) {
            return bufLen;
        } else {
            return bufLen - io.resid;
        }
    }
}

int scsi_write(int sg_fd, uint8_t *cmd, uint32_t cmdLen,
               uint8_t *buf, uint32_t bufLen) {

    sg_io_hdr_t io;
    uint8_t sense[32];
    int r;
    uint32_t i;

    memset(&io, 0, sizeof (io));

    io.interface_id = 'S';
    io.cmd_len = cmdLen;
    /* io.iovec_count = 0; */ /* memset takes care of this */
    io.mx_sb_len = sizeof (sense);
    io.dxfer_direction = SG_DXFER_TO_DEV;
    io.dxfer_len = bufLen;
    io.dxferp = buf;
    io.cmdp = cmd;
    io.sbp = sense;
    io.timeout = 20000; /* 20000 millisecs == 20 seconds */
    /* io.flags = 0; */ /* take defaults: indirect IO, etc */
    /* io.pack_id = 0; */
    /* io.usr_ptr = NULL; */

    /*  print debug scsi cmd */
    DPRINT("[S]\t\t\t\t\t >>> [");
    for (i = 0; i < cmdLen; ++i) {
        if (i > 0) {
            DPRINT(" ");
            if ((i%4) == 0 ) {
                DPRINT(" ");
            }
        }
        DPRINT("%02X", cmd[i]);
    }
    DPRINT("]\n");
    if (bufLen > 0) {
        /*  print debug write buffer */
        DPRINT("[S]\t\t\t\t\t >>> [");
        for (i = 0; i < 32 && i < bufLen; ++i) {
            if (i > 0) {
                DPRINT(" ");
                if (i % 16 == 0) {
                    DPRINT("\n\t\t\t\t\t      ");
                } else if ((i%4) == 0 ) {
                    DPRINT(" ");
                }
            }
            DPRINT("%02X", buf[i]);
        }
        DPRINT("]\n");
    }

    r = ioctl(sg_fd, SG_IO, &io);

    if (r == -1) {
        perror("ioctl");
        return PSLR_DEVICE_ERROR;
    }

    if ((io.info & SG_INFO_OK_MASK) != SG_INFO_OK) {
        print_scsi_error(&io, sense);
        return PSLR_SCSI_ERROR;
    } else {
        return PSLR_OK;
    }
}
