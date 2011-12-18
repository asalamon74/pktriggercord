/*
    pkTriggerCord
    Copyright (C) 2011 Andras Salamon <andras.salamon@melda.info>
    Remote control of Pentax DSLR cameras.

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
#ifndef PSLR_ENUM_H
#define PSLR_ENUM_H

typedef enum {
    PSLR_COLOR_SPACE_SRGB,
    PSLR_COLOR_SPACE_ADOBERGB,
    PSLR_COLOR_SPACE_MAX
} pslr_color_space_t;

typedef enum {
    PSLR_AF_MODE_MF,
    PSLR_AF_MODE_AF_S,
    PSLR_AF_MODE_AF_C,
    PSLR_AF_MODE_AF_A,
    PSLR_AF_MODE_MAX
} pslr_af_mode_t;

typedef enum {
    PSLR_AE_METERING_MULTI,
    PSLR_AE_METERING_CENTER,
    PSLR_AE_METERING_SPOT,
    PSLR_AE_METERING_MAX
} pslr_ae_metering_t;

typedef enum {
    PSLR_FLASH_MODE_MANUAL = 0,
    PSLR_FLASH_MODE_MANUAL_REDEYE = 1,
    PSLR_FLASH_MODE_SLOW = 2,
    PSLR_FLASH_MODE_SLOW_REDEYE = 3,
    PSLR_FLASH_MODE_TRAILING_CURTAIN = 4,
    PSLR_FLASH_MODE_AUTO = 5,
    PSLR_FLASH_MODE_AUTO_REDEYE = 6,
    // 7 not used
    PSLR_FLASH_MODE_WIRELESS = 8,
    PSLR_FLASH_MODE_MAX = 9
} pslr_flash_mode_t;

typedef enum {
    PSLR_DRIVE_MODE_SINGLE,
    PSLR_DRIVE_MODE_CONTINUOUS_HI,
    PSLR_DRIVE_MODE_SELF_TIMER_12,
    PSLR_DRIVE_MODE_SELF_TIMER_2,
    PSLR_DRIVE_MODE_REMOTE,
    PSLR_DRIVE_MODE_REMOTE_3,
    PSLR_DRIVE_MODE_CONTINUOUS_LO,
    PSLR_DRIVE_MODE_MAX
} pslr_drive_mode_t;

typedef enum {
    PSLR_AF_POINT_SEL_AUTO_5,
    PSLR_AF_POINT_SEL_SELECT,
    PSLR_AF_POINT_SEL_SPOT,
    PSLR_AF_POINT_SEL_AUTO_11, // maybe not for all cameras
    PSLR_AF_POINT_SEL_MAX
} pslr_af_point_sel_t;

typedef enum {
    PSLR_JPEG_IMAGE_TONE_NATURAL,
    PSLR_JPEG_IMAGE_TONE_BRIGHT,
    PSLR_JPEG_IMAGE_TONE_PORTRAIT,
    PSLR_JPEG_IMAGE_TONE_LANDSCAPE,
    PSLR_JPEG_IMAGE_TONE_VIBRANT,
    PSLR_JPEG_IMAGE_TONE_MONOCHROME,
    PSLR_JPEG_IMAGE_TONE_MUTED,
    PSLR_JPEG_IMAGE_TONE_REVERSAL_FILM,
    PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS,
    PSLR_JPEG_IMAGE_TONE_RADIANT,
    PSLR_JPEG_IMAGE_TONE_MAX
} pslr_jpeg_image_tone_t;

typedef enum {
    PSLR_WHITE_BALANCE_MODE_AUTO,
    PSLR_WHITE_BALANCE_MODE_DAYLIGHT,
    PSLR_WHITE_BALANCE_MODE_SHADE,
    PSLR_WHITE_BALANCE_MODE_CLOUDY,
    PSLR_WHITE_BALANCE_MODE_FLUORESCENT_DAYLIGHT_COLOR,
    PSLR_WHITE_BALANCE_MODE_FLUORESCENT_DAYLIGHT_WHITE,
    PSLR_WHITE_BALANCE_MODE_FLUORESCENT_COOL_WHITE,
    PSLR_WHITE_BALANCE_MODE_TUNGSTEN,
    PSLR_WHITE_BALANCE_MODE_FLASH,
    PSLR_WHITE_BALANCE_MODE_MANUAL,
    // a few items are missing here
    PSLR_WHITE_BALANCE_MODE_FLUORESCENT_WARM_WHITE = 0x0F,
    PSLR_WHITE_BALANCE_MODE_CTE = 0x10,
    PSLR_WHITE_BALANCE_MODE_MAX = 0x11
} pslr_white_balance_mode_t;

int find_in_array( const char** array, int length, char* str );

pslr_color_space_t get_pslr_color_space( char *str );
const char *get_pslr_color_space_str( pslr_color_space_t value );

pslr_af_mode_t get_pslr_af_mode( char *str );
const char *get_pslr_af_mode_str( pslr_af_mode_t value );

pslr_ae_metering_t get_pslr_ae_metering( char *str );
const char *get_pslr_ae_metering_str( pslr_ae_metering_t value );

pslr_flash_mode_t get_pslr_flash_mode( char *str );
const char *get_pslr_flash_mode_str( pslr_flash_mode_t value );

pslr_drive_mode_t get_pslr_drive_mode( char *str );
const char *get_pslr_drive_mode_str( pslr_drive_mode_t value );

pslr_af_point_sel_t get_pslr_af_point_sel( char *str );
const char *get_pslr_af_point_sel_str( pslr_af_point_sel_t value );

pslr_jpeg_image_tone_t get_pslr_jpeg_image_tone( char *str );
const char *get_pslr_jpeg_image_tone_str( pslr_jpeg_image_tone_t value );

pslr_white_balance_mode_t get_pslr_white_balance_mode( char *str );
const char *get_pslr_white_balance_mode_str( pslr_white_balance_mode_t value );

#endif
