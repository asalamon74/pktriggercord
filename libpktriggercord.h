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
#ifdef RAD10
#include <utime.h>
#include <io.h>
#include "tdbtimes.h"
#include "getopt.h"
#else
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#endif
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <math.h>

#include "pslr.h"
#include "pktriggercord-servermode.h"

#ifdef WIN32
#define FILE_ACCESS O_WRONLY | O_CREAT | O_TRUNC | O_BINARY
#else
#define FILE_ACCESS O_WRONLY | O_CREAT | O_TRUNC
#endif

extern bool debug;

#endif