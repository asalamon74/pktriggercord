/*
    pkTriggerCord
    Copyright (C) 2011 Andras Salamon <andras.salamon@melda.info>
    Remote control of Pentax DSLR cameras.

    Support for K200D added by Jens Dreyer <jens.dreyer@udo.edu> 04/2011
    Support for K-r added by Vincenc Podobnik <vincenc.podobnik@gmail.com> 06/2011

    based on:

    PK-Remote
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2008 Pontus Lidman <pontus@lysator.liu.se>

    PK-Remote for Windows
    Copyright (C) 2010 Tomasz Kos
        
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
#include "pslr_enum.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>

const char* pslr_color_space_str[PSLR_COLOR_SPACE_MAX] = {
    "sRGB",
    "AdobeRGB"
};

const char* pslr_af_mode_str[PSLR_AF_MODE_MAX] = {
    "MF",
    "AF.S",
    "AF.C",
    "AF.A"
};


// case insenstive comparison
// strnicmp
int str_comparison_i (const char *s1, const char *s2, int n)
{
    if( s1 == NULL ) {
	return s2 == NULL ? 0 : -(*s2);
    }
    if (s2 == NULL) {
	return *s1;
    }

    char c1='\0', c2='\0';
    int length=0;
    while( length<n && (c1 = tolower (*s1)) == (c2 = tolower (*s2))) {
	if (*s1 == '\0') break;
	++s1; 
	++s2;
	++length;
    }
    return c1 - c2;
}

int find_in_array( const char** array, int length, char* str ) {
    int i;
    for( i = 0; i<length; ++i ) {
	if( str_comparison_i( array[i], str, strlen(array[i]) ) == 0 ) {
	    return i;
	}
    }
    return -1;
}

pslr_color_space_t get_pslr_color_space( char *str ) {
    return find_in_array( pslr_color_space_str, sizeof(pslr_color_space_str)/sizeof(pslr_color_space_str[0]),str);
}

const char *get_pslr_color_space_str( pslr_color_space_t value ) {
    return pslr_color_space_str[value];
}

pslr_af_mode_t get_pslr_af_mode( char *str ) {
    return find_in_array( pslr_af_mode_str, sizeof(pslr_af_mode_str)/sizeof(pslr_af_mode_str[0]),str);
}

const char *get_pslr_af_mode_str( pslr_af_mode_t value ) {
    return pslr_af_mode_str[value];
}

