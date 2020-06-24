/*
    pkTriggerCord
    Copyright (C) 2011-2020 Andras Salamon <andras.salamon@melda.info>
    Remote control of Pentax DSLR cameras.

    Support for K200D added by Jens Dreyer <jens.dreyer@udo.edu> 04/2011
    Support for K-r added by Vincenc Podobnik <vincenc.podobnik@gmail.com> 06/2011
    Support for K-30 added by Camilo Polymeris <cpolymeris@gmail.com> 09/2012
    Support for K-01 added by Ethan Queen <ethanqueen@gmail.com> 01/2013
    Support for K-3 added by Tao Wang <twang2218@gmail.com> 01/2016

    based on:

    PK-Remote
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2008 Pontus Lidman <pontus@lysator.liu.se>

    PK-Remote for Windows
    Copyright (C) 2010 Tomasz Kos

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

#ifndef PSLR_SHARED_H
#define PSLR_SHARED_H

// Generic helper definitions for shared library support
#ifdef WIN32
#define PK_HELPER_LIB_IMPORT    __declspec(dllimport)
#define PK_HELPER_LIB_EXPORT    __declspec(dllexport)
#define PK_HELPER_LOCAL
#else
#define PK_HELPER_LIB_IMPORT    __attribute__ ((visibility ("default")))
#define PK_HELPER_LIB_EXPORT    __attribute__ ((visibility ("default")))
#define PK_HELPER_LOCAL         __attribute__ ((visibility ("hidden")))
#endif

#ifdef PK_LIB_EXPORTS
// defined if pktriggercord is compiled as a shared lib
#define PK_API          PK_HELPER_LIB_EXPORT
#define PK_API_LOCAL    PK_HELPER_LOCAL
#else // PK_LIB_EXPORTS
#ifdef PK_LIB_STATIC
// defined if pktriggercord is compiled or used as a static lib
#define PK_API
#define PK_API_LOCAL
#else // PK_LIB_STATIC
// defined if pktriggercord is used as a shared lib.
// This is the default, non specified option so that an external code does not
// need to know about this mechanics and can just import and use the header
#define PK_API          PK_HELPER_LIB_IMPORT
#define PK_API_LOCAL    PK_HELPER_LOCAL
#endif // PK_LIB_STATIC
#endif // PK_LIB_EXPORTS

#endif