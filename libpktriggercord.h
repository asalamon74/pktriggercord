/*
    Header file for libpktriggercord
    Shared library wrapper for pkTriggerCord
    Copyright (c) 2020 Karl Rees

    for:

    pkTriggerCord
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2011-2019 Andras Salamon <andras.salamon@melda.info>

    which is based on:

    pslr-shoot

    Command line remote control of Pentax DSLR cameras.
    Copyright (C) 2009 Ramiro Barreiro <ramiro_barreiro69@yahoo.es>
    With fragments of code from PK-Remote by Pontus Lidman.
    <https://sourceforge.net/projects/pkremote>

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

#ifndef LIBPKTRIGGERCORD_H
#define LIBPKTRIGGERCORD_H

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <math.h>
#include <sys/time.h>

#include "pslr.h"
#include "pktriggercord-servermode.h"

#ifdef WIN32
#define FILE_ACCESS O_WRONLY | O_CREAT | O_TRUNC | O_BINARY
#else
#define FILE_ACCESS O_WRONLY | O_CREAT | O_TRUNC
#endif

extern bool debug;
extern bool warnings;

extern bool bulb_timer_before;
extern bool astrotracer_before;


int save_buffer(pslr_handle_t camhandle, int bufno, int fd, pslr_status *status, user_file_format filefmt, int jpeg_stars);
void save_memory(pslr_handle_t camhandle, int fd, uint32_t length);

void print_status_info( pslr_handle_t h, pslr_status status );
void print_settings_info( pslr_handle_t h, pslr_settings settings );

int open_file(char* output_file, int frameNo, user_file_format_t ufft);
void warning_message( const char* message, ... );

void process_wbadj( const char* argv0, const char chr, uint32_t adj, uint32_t *wbadj_mg, uint32_t *wbadj_ba );

void bulb_old(pslr_handle_t camhandle, pslr_rational_t shutter_speed, struct timeval prev_time);
void bulb_new(pslr_handle_t camhandle, pslr_rational_t shutter_speed);
void bulb_new_cleanup(pslr_handle_t camhandle);

#endif