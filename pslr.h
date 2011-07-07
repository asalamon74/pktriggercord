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

#ifndef PSLR_H
#define PSLR_H

#include "pslr_scsi.h"

#define MAX_RESOLUTION_SIZE 4

#define PSLR_LIGHT_METER_AE_LOCK 0x8

#define PSLR_AF_POINT_TOP_LEFT   0x1
#define PSLR_AF_POINT_TOP_MID    0x2
#define PSLR_AF_POINT_TOP_RIGHT  0x4
#define PSLR_AF_POINT_FAR_LEFT   0x8
#define PSLR_AF_POINT_MID_LEFT   0x10
#define PSLR_AF_POINT_MID_MID    0x20
#define PSLR_AF_POINT_MID_RIGHT  0x40
#define PSLR_AF_POINT_FAR_RIGHT  0x80
#define PSLR_AF_POINT_BOT_LEFT   0x100
#define PSLR_AF_POINT_BOT_MID    0x200
#define PSLR_AF_POINT_BOT_RIGHT  0x400

#define PSLR_ID1_K20D    0x12cd2
#define PSLR_ID1_K10D    0x12c1e
#define PSLR_ID1_K110D   0x12b9d
#define PSLR_ID1_K100D   0x12b9c
#define PSLR_ID1_IST_D   0x12994
#define PSLR_ID1_IST_DS  0x12aa2
#define PSLR_ID1_IST_DS2 0x12b60
#define PSLR_ID1_IST_DL  0x12b1a
#define PSLR_ID1_GX10    0x12c20
#define PSLR_ID1_GX20    0x12cd4
#define PSLR_ID1_KX      0x12dfe
#define PSLR_ID1_K200D   0x12cfa
#define PSLR_ID1_K7      0x12db8
#define PSLR_ID1_KR      0x12e6c
#define PSLR_ID1_K5      0x12e76

#define MAX_STATUS_BUF_SIZE 444

typedef enum {
    PSLR_BUF_PEF,
    PSLR_BUF_DNG,
    PSLR_BUF_JPEG_4, // K20D only
    PSLR_BUF_JPEG_3,
    PSLR_BUF_JPEG_2,
    PSLR_BUF_JPEG_1,
    PSLR_BUF_PREVIEW = 8,
    PSLR_BUF_THUMBNAIL = 9
} pslr_buffer_type;

/*typedef enum {
    PSLR_JPEG_QUALITY_4, // K20D only
    PSLR_JPEG_QUALITY_3,
    PSLR_JPEG_QUALITY_2,
    PSLR_JPEG_QUALITY_1,
    PSLR_JPEG_QUALITY_MAX
    } pslr_jpeg_quality_t;*/

typedef enum {
    PSLR_JPEG_IMAGE_MODE_NATURAL,
    PSLR_JPEG_IMAGE_MODE_BRIGHT,
    PSLR_JPEG_IMAGE_MODE_PORTRAIT,
    PSLR_JPEG_IMAGE_MODE_LANDSCAPE,
    PSLR_JPEG_IMAGE_MODE_VIBRANT,
    PSLR_JPEG_IMAGE_MODE_MONOCHROME,
    PSLR_JPEG_IMAGE_MODE_MUTED,
    PSLR_JPEG_IMAGE_MODE_REVERSAL_FILM,
    PSLR_JPEG_IMAGE_MODE_MAX
} pslr_jpeg_image_mode_t;

typedef enum {
    PSLR_COLOR_SPACE_SRGB,
    PSLR_COLOR_SPACE_ADOBERGB,
    PSLR_COLOR_SPACE_MAX
} pslr_color_space_t;


typedef enum {
    PSLR_RAW_FORMAT_PEF,
    PSLR_RAW_FORMAT_DNG,
    PSLR_RAW_FORMAT_MAX
} pslr_raw_format_t;

typedef enum {
    PSLR_IMAGE_FORMAT_JPEG,
    PSLR_IMAGE_FORMAT_RAW,
    PSLR_IMAGE_FORMAT_RAW_PLUS,
    PSLR_IMAGE_FORMAT_MAX
} pslr_image_format_t;

typedef enum {
    USER_FILE_FORMAT_PEF,
    USER_FILE_FORMAT_DNG,
    USER_FILE_FORMAT_JPEG,
    USER_FILE_FORMAT_MAX
} user_file_format;

typedef struct {
    user_file_format uff;
    const char *file_format_name;
    const char *extension;
} user_file_format_t;

user_file_format_t file_formats[3];

user_file_format_t *get_file_format_t( user_file_format uff );

typedef enum {
    PSLR_CUSTOM_EV_STEPS_1_2,
    PSLR_CUSTOM_EV_STEPS_1_3,
    PSLR_CUSTOM_EV_STEPS_MAX
} pslr_custom_ev_steps_t;

typedef enum {
    PSLR_CUSTOM_SENSITIVITY_STEPS_1EV,
    PSLR_CUSTOM_SENSITIVITY_STEPS_AS_EV,
    PSLR_CUSTOM_SENSITIVITY_STEPS_MAX
} pslr_custom_sensitivity_steps_t;

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
    PSLR_EXPOSURE_MODE_GREEN = 1,
    PSLR_EXPOSURE_MODE_P = 0 ,
    PSLR_EXPOSURE_MODE_SV = 15,
    PSLR_EXPOSURE_MODE_TV = 4,
    PSLR_EXPOSURE_MODE_AV = 5,
    PSLR_EXPOSURE_MODE_TAV, // ?
    PSLR_EXPOSURE_MODE_M = 8,
    PSLR_EXPOSURE_MODE_B = 9,
    PSLR_EXPOSURE_MODE_X, // ?   
    PSLR_EXPOSURE_MODE_MAX = 16

} pslr_exposure_mode_t;

typedef enum {
    PSLR_GUI_EXPOSURE_MODE_GREEN,
    PSLR_GUI_EXPOSURE_MODE_P,
    PSLR_GUI_EXPOSURE_MODE_SV,
    PSLR_GUI_EXPOSURE_MODE_TV,
    PSLR_GUI_EXPOSURE_MODE_AV,
    PSLR_GUI_EXPOSURE_MODE_TAV,
    PSLR_GUI_EXPOSURE_MODE_M,
    PSLR_GUI_EXPOSURE_MODE_B,
    PSLR_GUI_EXPOSURE_MODE_X,
    PSLR_GUI_EXPOSURE_MODE_MAX
} pslr_gui_exposure_mode_t;

typedef enum {
    PSLR_AF_POINT_SEL_AUTO_5,
    PSLR_AF_POINT_SEL_SELECT,
    PSLR_AF_POINT_SEL_SPOT,
    PSLR_AF_POINT_SEL_AUTO_11, // maybe not for all cameras
    PSLR_AF_POINT_SEL_MAX
} pslr_af_point_sel_t;

typedef enum {
    PSLR_AF_MODE_MF,
    PSLR_AF_MODE_AF_S,
    PSLR_AF_MODE_AF_C,
    PSLR_AF_MODE_AF_A,
    PSLR_AF_MODE_MAX,
} pslr_af_mode_t;

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

typedef enum {
    PSLR_FLASH_MODE_MANUAL = 0,
    PSLR_FLASH_MODE_MANUAL_REDEYE = 1,
    PSLR_FLASH_MODE_SLOW = 2,
    PSLR_FLASH_MODE_SLOW_REDEYE = 3,
    // 4 ?
    PSLR_FLASH_MODE_AUTO = 5,
    PSLR_FLASH_MODE_AUTO_REDEYE = 6,
    PSLR_FLASH_MODE_TRAILING_CURTAIN = 7,
    PSLR_FLASH_MODE_WIRELESS = 8,
    PSLR_FLASH_MODE_MAX = 9
} pslr_flash_mode_t;


typedef struct {
    int32_t nom;
    int32_t denom;
} pslr_rational_t;

typedef void *pslr_handle_t;

typedef enum {
    PSLR_STREAM_RAW,
    PSLR_STREAM_JPEG
} pslr_stream_t;

typedef void *pslr_buffer_handle_t;

typedef struct {
    uint16_t power;
} pslr_status_brief;

typedef struct {
    pslr_status_brief brief;
    uint16_t bufmask;
    uint32_t current_iso;
    pslr_rational_t current_shutter_speed;
    pslr_rational_t current_aperture;
    pslr_rational_t lens_max_aperture;
    pslr_rational_t lens_min_aperture;
    pslr_rational_t set_shutter_speed;
    pslr_rational_t set_aperture;
    pslr_rational_t max_shutter_speed;
    uint32_t auto_bracket_mode; // 1: on, 0: off
    pslr_rational_t auto_bracket_ev;
    uint32_t auto_bracket_picture_count;
    uint32_t fixed_iso;
    uint32_t jpeg_resolution;
    uint32_t jpeg_saturation;
    uint32_t jpeg_quality;
    uint32_t jpeg_contrast;
    uint32_t jpeg_sharpness;
    uint32_t jpeg_image_mode;
    uint32_t jpeg_hue;
    pslr_rational_t zoom;
    int32_t focus;
    uint32_t image_format;
    uint32_t raw_format;
    uint32_t light_meter_flags;
    pslr_rational_t ec;
    uint32_t custom_ev_steps;
    uint32_t custom_sensitivity_steps;
    uint32_t exposure_mode;
    uint32_t user_mode_flag;
    uint32_t af_mode;
    uint32_t af_point_select;
    uint32_t selected_af_point;
    uint32_t focused_af_point;
    uint32_t auto_iso_min;
    uint32_t auto_iso_max;
    uint32_t drive_mode;
    uint32_t shake_reduction;
    uint32_t white_balance_mode;
    uint32_t white_balance_adjust_mg;
    uint32_t white_balance_adjust_ba;
    uint32_t flash_mode;
    int32_t flash_exposure_compensation; // 1/256
    int32_t manual_mode_ev; // 1/10
    uint32_t color_space;
    uint32_t lens_id1;
    uint32_t lens_id2;
} pslr_status;

typedef struct {
    uint32_t a;
    uint32_t b;
    uint32_t addr;
    uint32_t length;
} pslr_buffer_segment_info;

typedef void (*pslr_progress_callback_t)(uint32_t current, uint32_t total);

pslr_handle_t pslr_init();
int pslr_connect(pslr_handle_t h);
int pslr_disconnect(pslr_handle_t h);
int pslr_shutdown(pslr_handle_t h);
const char *pslr_model(uint32_t id);

int pslr_shutter(pslr_handle_t h);
int pslr_focus(pslr_handle_t h);

int pslr_get_status(pslr_handle_t h, pslr_status *sbuf);
int pslr_get_status_buffer(pslr_handle_t h, uint8_t *st_buf);

int pslr_get_buffer(pslr_handle_t h, int bufno, pslr_buffer_type type, int resolution,
                    uint8_t **pdata, uint32_t *pdatalen);

int pslr_set_progress_callback(pslr_handle_t h, pslr_progress_callback_t cb, 
                               uintptr_t user_data);

int pslr_set_shutter(pslr_handle_t h, pslr_rational_t value);
int pslr_set_aperture(pslr_handle_t h, pslr_rational_t value);
int pslr_set_iso(pslr_handle_t h, uint32_t value, uint32_t auto_min_value, uint32_t auto_max_value);
int pslr_set_ec(pslr_handle_t h, pslr_rational_t value);

int pslr_set_white_balance(pslr_handle_t h, pslr_white_balance_mode_t wb_mode, uint32_t wb_adjust_mg, uint32_t wb_adjust_ba);
int pslr_set_flash_mode(pslr_handle_t h, pslr_flash_mode_t value);
int pslr_set_flash_exposure_compensation(pslr_handle_t h, pslr_rational_t value);
int pslr_set_drive_mode(pslr_handle_t h, pslr_drive_mode_t drive_mode);
int pslr_set_af_mode(pslr_handle_t h, pslr_af_mode_t af_mode);

int pslr_set_jpeg_stars(pslr_handle_t h, int jpeg_stars);
int pslr_set_jpeg_resolution(pslr_handle_t h, int megapixel);
int pslr_set_jpeg_image_mode(pslr_handle_t h, pslr_jpeg_image_mode_t image_mode);

int pslr_set_jpeg_sharpness(pslr_handle_t h, int32_t sharpness);
int pslr_set_jpeg_contrast(pslr_handle_t h, int32_t contrast);
int pslr_set_jpeg_saturation(pslr_handle_t h, int32_t saturation);
int pslr_set_jpeg_hue(pslr_handle_t h, int32_t hue);

int pslr_set_image_format(pslr_handle_t h, pslr_image_format_t format);
int pslr_set_raw_format(pslr_handle_t h, pslr_raw_format_t format);

int pslr_delete_buffer(pslr_handle_t h, int bufno);

int pslr_green_button(pslr_handle_t h);
int pslr_ae_lock(pslr_handle_t h, bool lock);

int pslr_buffer_open(pslr_handle_t h, int bufno, pslr_buffer_type type, int resolution);
uint32_t pslr_buffer_read(pslr_handle_t h, uint8_t *buf, uint32_t size);
void pslr_buffer_close(pslr_buffer_handle_t h);
uint32_t pslr_buffer_get_size(pslr_handle_t h);

int pslr_set_exposure_mode(pslr_handle_t h, pslr_exposure_mode_t mode);
int pslr_select_af_point(pslr_handle_t h, uint32_t point);

const char *pslr_camera_name(pslr_handle_t h);
int pslr_get_model_jpeg_stars(pslr_handle_t h);
int pslr_get_model_jpeg_property_levels(pslr_handle_t h);
int pslr_get_model_buffer_size(pslr_handle_t h);
int pslr_get_model_fastest_shutter_speed(pslr_handle_t h);
int pslr_get_model_base_iso_min(pslr_handle_t h);
int pslr_get_model_base_iso_max(pslr_handle_t h);
int *pslr_get_model_jpeg_resolutions(pslr_handle_t h);
bool pslr_get_model_only_limited(pslr_handle_t h);

pslr_buffer_type pslr_get_jpeg_buffer_type(pslr_handle_t h, int quality);
int pslr_get_jpeg_resolution(pslr_handle_t h, int hwres);

int get_hw_jpeg_quality( pslr_handle_t h, int jpeg_stars);

void hexdump(uint8_t *buf, uint32_t bufLen);

int pslr_test( pslr_handle_t h, bool cmd9_wrap, int subcommand, int argnum,  int arg1, int arg2, int arg3);

void write_debug( const char* message, ... );

#endif
