/*
    pkTriggerCord
    Copyright (C) 2011 Andras Salamon <andras.salamon@melda.info>
    Remote control of Pentax DSLR cameras.

    Support for K200D added by Jens Dreyer <jens.dreyer@udo.edu> 04/2011

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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <dirent.h>

#include "pslr.h"
#include "pslr_scsi.h"

#define MAX_SEGMENTS 4
#define POLL_INTERVAL 100000 /* Number of us to wait when polling */
#define BLKSZ 65536 /* Block size for downloads; if too big, we get
                     * memory allocation error from sg driver */
#define BLOCK_RETRY 3 /* Number of retries, since we can occasionally
                       * get SCSI errors when downloading data */
#define MAX_STATUS_BUF_SIZE 436

#define CHECK(x) do {                           \
        int __r;                                \
        __r = (x);                                                      \
        if (__r != PSLR_OK) {                                           \
            fprintf(stderr, "%s:%d:%s failed: %d\n", __FILE__, __LINE__, #x, __r); \
            return __r;                                                 \
        }                                                               \
    } while (0)

#define CHECK_CLN(x,rval,label) do {            \
        int __r;                                                        \
        __r = (x);                                                      \
        if (__r != PSLR_OK) {                                           \
            fprintf(stderr, "%s:%d:%s failed: %d\n", __FILE__, __LINE__, #x, __r); \
            rval = __r;                                                 \
            goto label;                                                 \
        }                                                               \
    } while (0)

typedef struct {
    uint32_t offset;
    uint32_t addr;
    uint32_t length;
} ipslr_segment_t;

typedef struct ipslr_handle ipslr_handle_t;

typedef int (*ipslr_status_parse_t)(ipslr_handle_t *p, pslr_status *status, int n);

typedef struct {
    uint32_t id1;
//    uint32_t id2;
    const char *name;
    bool old_scsi_command; // 0 for *ist cameras, 1 for the newer cameras
    int buffer_size;
    int jpeg_stars; // 3 or 4
    int jpeg_resolutions[MAX_RESOLUTION_SIZE];
    int jpeg_property_levels; // 7 or 9
    int fastest_shutter_speed;
    ipslr_status_parse_t parser_function;
} ipslr_model_info_t;

struct ipslr_handle {
    int fd;
    pslr_status status;
    uint32_t id1;
//    uint32_t id2;
    ipslr_model_info_t *model;
    ipslr_segment_t segments[MAX_SEGMENTS];
    uint32_t segment_count;
    uint32_t offset;
    uint8_t status_buffer[MAX_STATUS_BUF_SIZE];
};


ipslr_handle_t pslr;

static int ipslr_set_mode(ipslr_handle_t *p, uint32_t mode);
static int ipslr_cmd_00_09(ipslr_handle_t *p, uint32_t mode);
static int ipslr_cmd_10_0a(ipslr_handle_t *p, uint32_t mode);
static int ipslr_cmd_00_05(ipslr_handle_t *p);
static int ipslr_status(ipslr_handle_t *p, uint8_t *buf);
static int ipslr_status_full(ipslr_handle_t *p, pslr_status *status);
static int ipslr_press_shutter(ipslr_handle_t *p);
static int ipslr_select_buffer(ipslr_handle_t *p, int bufno, pslr_buffer_type buftype, int bufres);
static int ipslr_buffer_segment_info(ipslr_handle_t *p, pslr_buffer_segment_info *pInfo);
//static int ipslr_read_buffer(ipslr_handle_t *p, int bufno, pslr_buffer_type buftype, int bufres,
//        uint8_t **ppData, uint32_t *pLen);
static int ipslr_next_segment(ipslr_handle_t *p);
static int ipslr_download(ipslr_handle_t *p, uint32_t addr, uint32_t length, uint8_t *buf);
static int ipslr_identify(ipslr_handle_t *p);
static int ipslr_write_args(ipslr_handle_t *p, int n, ...);

//static int ipslr_cmd_00_04(ipslr_handle_t *p, uint32_t mode);

static int command(int fd, int a, int b, int c);
static int get_status(int fd);
static int get_result(int fd);
static int read_result(int fd, uint8_t *buf, uint32_t n);

void hexdump(uint8_t *buf, uint32_t bufLen);

static uint32_t get_uint32(uint8_t *buf);
static int32_t get_int32(uint8_t *buf);

static pslr_progress_callback_t progress_callback = NULL;

user_file_format_t file_formats[3] = {
    { USER_FILE_FORMAT_PEF, "PEF", "pef"},
    { USER_FILE_FORMAT_DNG, "DNG", "dng"},
    { USER_FILE_FORMAT_JPEG, "JPEG", "jpg"},
};

int ipslr_status_parse_kx(ipslr_handle_t *p, pslr_status *status, int n);
int ipslr_status_parse_k20d(ipslr_handle_t *p, pslr_status *status, int n);
int ipslr_status_parse_k10d(ipslr_handle_t *p, pslr_status *status, int n);
int ipslr_status_parse_k200d(ipslr_handle_t *p, pslr_status *status, int n);
int ipslr_status_parse_istds(ipslr_handle_t *p, pslr_status *status, int n);

static ipslr_model_info_t camera_models[] = {
    { PSLR_ID1_K20D,    "K20D",     0, 412, 4, {14, 10, 6, 2}, 7, 4000, ipslr_status_parse_k20d},
    { PSLR_ID1_K10D,    "K10D",     0, 392, 3, {10, 6, 2},     7, 4000, ipslr_status_parse_k10d},
    { PSLR_ID1_K110D,   "K110D",    0, 0,   0, {}, 0, 0, NULL},
    { PSLR_ID1_K100D,   "K100D",    0, 0,   0, {}, 0, 0, NULL},
    { PSLR_ID1_IST_DS2, "*ist DS2", 1, 0,   0, {}, 0, 0, NULL},
    { PSLR_ID1_IST_DL,  "*ist DL",  1, 0,   0, {}, 0, 0, NULL},
    { PSLR_ID1_IST_DS,  "*ist DS",  1, 264, 3, {6, 4, 2},      7, 4000, ipslr_status_parse_istds},
    { PSLR_ID1_IST_D,   "*ist D",   1, 0,   0, {}, 0, 0, NULL},
    { PSLR_ID1_GX10,    "GX10",     0, 392, 3, {10, 6, 2},     7, 4000, ipslr_status_parse_k10d},
    { PSLR_ID1_GX20,    "GX20",     0, 412, 4, {14, 10, 6, 2}, 7, 4000, ipslr_status_parse_k20d},
    { PSLR_ID1_KX,      "K-x",      0, 436, 3, {12, 10, 6, 2}, 9, 6000, ipslr_status_parse_kx},
    { PSLR_ID1_K200D,   "K200D",    0, 408, 3, {10, 6, 2},     9, 4000, ipslr_status_parse_k200d}, 
    { PSLR_ID1_K7,      "K-7",      0, 436, 4, {14, 10, 6, 2}, 9, 8000, ipslr_status_parse_kx},
};

char* valid_vendors[2] = {"PENTAX", "SAMSUNG"};
char* valid_models[5] = {"DIGITAL_CAMERA", "DSC_K20D", "DSC_K-x", "DSC_K200D", "DSC_K-7"};

user_file_format_t *get_file_format_t( user_file_format uff ) {
    int i;    
    for (i = 0; i<sizeof(file_formats) / sizeof(file_formats[0]); i++) {
        if (file_formats[i].uff == uff) {
            return &file_formats[i];
        }
    }
    return NULL;
}


static pslr_gui_exposure_mode_t exposure_mode_conversion( pslr_exposure_mode_t exp ) {
  switch( exp ) {
    
    case PSLR_EXPOSURE_MODE_GREEN:
      return PSLR_GUI_EXPOSURE_MODE_GREEN;
    case PSLR_EXPOSURE_MODE_P:
      return PSLR_GUI_EXPOSURE_MODE_P;
    case PSLR_EXPOSURE_MODE_SV:
      return PSLR_GUI_EXPOSURE_MODE_SV;
    case PSLR_EXPOSURE_MODE_TV:
      return PSLR_GUI_EXPOSURE_MODE_TV;
    case PSLR_EXPOSURE_MODE_AV:
      return PSLR_GUI_EXPOSURE_MODE_AV;
    case PSLR_EXPOSURE_MODE_TAV:
      return PSLR_GUI_EXPOSURE_MODE_TAV;
    case PSLR_EXPOSURE_MODE_M:
      return PSLR_GUI_EXPOSURE_MODE_M;
    case PSLR_EXPOSURE_MODE_B:
      return PSLR_GUI_EXPOSURE_MODE_B;
    case PSLR_EXPOSURE_MODE_X:
      return PSLR_GUI_EXPOSURE_MODE_X;
    case PSLR_EXPOSURE_MODE_MAX:
      return PSLR_GUI_EXPOSURE_MODE_MAX;
  }    
  return 0;
}

int find_in_array( char** array, char* str ) {
    int i;
    for( i = 0; i<sizeof(array); ++i ) {
	if( strncmp( array[i], str, sizeof(array[i]) ) == 0 ) {
	    return i;
	}
    }
    return -1;
}

pslr_handle_t pslr_init() {
    int fd;
    char vendorId[20];
    char productId[20];
    int driveNum;
    char **drives;

    drives = get_drives(&driveNum);
    int i;
    for( i=0; i<driveNum; ++i ) {
	pslr_result result = get_drive_info( drives[i], &fd, vendorId, sizeof(vendorId), productId, sizeof(productId));

	DPRINT("Checking drive: %s %s\n", vendorId, productId);
	if( result == PSLR_OK && find_in_array( valid_vendors, vendorId) != -1 
	    && find_in_array( valid_models, productId) != -1 ) {
	    DPRINT("Found camera %s %s\n", vendorId, productId);
	    pslr.fd = fd;
	    /* Only support first connected camera at this time. */
	    return &pslr;	    
	} else {
	    close( fd );
	    continue;
	}
    }
    return NULL;
}

int pslr_connect(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    uint8_t statusbuf[16];
    CHECK(ipslr_status(p, statusbuf));
    CHECK(ipslr_set_mode(p, 1));
    CHECK(ipslr_status(p, statusbuf));
    CHECK(ipslr_identify(p));
    CHECK(ipslr_status_full(p, &p->status));
    DPRINT("init bufmask=0x%x\n", p->status.bufmask);
    if( !p->model->old_scsi_command ) {
        CHECK(ipslr_cmd_00_09(p, 2));
    }
    CHECK(ipslr_status_full(p, &p->status));
    CHECK(ipslr_cmd_10_0a(p, 1));
    if( p->model->old_scsi_command ) {
        CHECK(ipslr_cmd_00_05(p));
    }

    CHECK(ipslr_status_full(p, &p->status));
    return 0;
}

int pslr_disconnect(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    uint8_t statusbuf[16];
    CHECK(ipslr_cmd_10_0a(p, 0));
    CHECK(ipslr_set_mode(p, 0));
    CHECK(ipslr_status(p, statusbuf));
    return PSLR_OK;
}

int pslr_shutdown(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    close(p->fd);
    return PSLR_OK;
}

int pslr_shutter(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    ipslr_press_shutter(p);
    return PSLR_OK;
}

int pslr_focus(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    CHECK(ipslr_write_args(p, 1, 1));
    CHECK(command(p->fd, 0x10, 0x05, 0x04));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

int pslr_get_status(pslr_handle_t h, pslr_status *ps) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    memset( ps, 0, sizeof( pslr_status ));
    CHECK(ipslr_status_full(p, &p->status));
    memcpy(ps, &p->status, sizeof (pslr_status));
    return PSLR_OK;
}

int pslr_get_status_buffer(pslr_handle_t h, uint8_t *st_buf) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    memset( st_buf, 0, MAX_STATUS_BUF_SIZE);
//    CHECK(ipslr_status_full(p, &p->status));
    ipslr_status_full(p, &p->status);
    memcpy(st_buf, p->status_buffer, MAX_STATUS_BUF_SIZE);
    return PSLR_OK;
}

int pslr_get_buffer(pslr_handle_t h, int bufno, pslr_buffer_type type, int resolution,
        uint8_t **ppData, uint32_t *pLen) {
    uint8_t *buf = 0;
    int ret;
    ret = pslr_buffer_open(h, bufno, type, resolution);
    if( ret != PSLR_OK ) {
	return ret;
    }

    uint32_t size = pslr_buffer_get_size(h);
    buf = malloc(size); 
    if (!buf) {
	return PSLR_NO_MEMORY;
    }

    int bytes = pslr_buffer_read(h, buf, size);

    if( bytes != size ) {
	return PSLR_READ_ERROR;
    }
    pslr_buffer_close(h);
    if (ppData) {
	*ppData = buf;
    }
    if (pLen) {
	*pLen = size;
    }    

    return PSLR_OK;
}

int pslr_set_progress_callback(pslr_handle_t h, pslr_progress_callback_t cb, uintptr_t user_data) {
    progress_callback = cb;
    return PSLR_OK;
}

int ipslr_handle_command_x18( ipslr_handle_t *p, bool cmd9_wrap, int subcommand, int argnum,  int arg1, int arg2, int arg3) {
    if( cmd9_wrap ) {
        CHECK(ipslr_cmd_00_09(p, 1));
    }
    CHECK(ipslr_write_args(p, argnum, arg1, arg2, arg3));
    CHECK(command(p->fd, 0x18, subcommand, 4 * argnum));
    CHECK(get_status(p->fd));
    if( cmd9_wrap ) {
        CHECK(ipslr_cmd_00_09(p, 2));
    }
    return PSLR_OK;
}

#ifdef DEBUG
int pslr_test( pslr_handle_t h, bool cmd9_wrap, int subcommand, int argnum,  int arg1, int arg2, int arg3) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, cmd9_wrap, subcommand, argnum, arg1, arg2, arg3);
}
#endif

int pslr_set_shutter(pslr_handle_t h, pslr_rational_t value) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, 0x16, 2, value.nom, value.denom, 0);
}

int pslr_set_aperture(pslr_handle_t h, pslr_rational_t value) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, false, 0x17, 3, value.nom, value.denom, 0);
}

int pslr_set_iso(pslr_handle_t h, uint32_t value, uint32_t auto_min_value, uint32_t auto_max_value) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, 0x15, 3, value, auto_min_value, auto_max_value);
}

int pslr_set_ec(pslr_handle_t h, pslr_rational_t value) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, 0x18, 2, value.nom, value.denom, 0);
}

int pslr_set_white_balance(pslr_handle_t h, pslr_white_balance_mode_t wb_mode, uint32_t wb_adjust_mg, uint32_t wb_adjust_ba) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, 0x10, 3, wb_mode, wb_adjust_mg, wb_adjust_ba);
}

int pslr_set_flash_mode(pslr_handle_t h, pslr_flash_mode_t value) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, 0x04, 1, value, 0, 0);
}

int pslr_set_flash_exposure_compensation(pslr_handle_t h, pslr_rational_t value) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, 0x1a, 2, value.nom, value.denom, 0);    
}

int pslr_set_drive_mode(pslr_handle_t h, pslr_drive_mode_t drive_mode) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, 0x1c, 1, drive_mode, 0, 0);    
}

int pslr_set_af_mode(pslr_handle_t h, pslr_af_mode_t af_mode) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, 0x06, 1, af_mode, 0, 0);    
}

int _get_hw_jpeg_quality( ipslr_model_info_t *model, pslr_jpeg_quality_t quality) {
    return quality - (PSLR_JPEG_QUALITY_MAX - model->jpeg_stars);
}

pslr_jpeg_quality_t _get_user_jpeg_quality( ipslr_model_info_t *model, int hwqual ) {
    return hwqual + (PSLR_JPEG_QUALITY_MAX - model->jpeg_stars);
}

int pslr_set_jpeg_quality(pslr_handle_t h, pslr_jpeg_quality_t quality) {
    int hwqual;
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (quality >= PSLR_JPEG_QUALITY_MAX) {
        return PSLR_PARAM;
    }
    hwqual = _get_hw_jpeg_quality( p->model, quality );
    return ipslr_handle_command_x18( p, true, 0x13, 2, 1, hwqual, 0);
}

int _get_user_jpeg_resolution( ipslr_model_info_t *model, int hwres ) {
    return model->jpeg_resolutions[hwres];
}

int pslr_get_jpeg_resolution(pslr_handle_t h, int hwres) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return _get_user_jpeg_resolution( p->model, hwres );
}

int _get_hw_jpeg_resolution( ipslr_model_info_t *model, int megapixel) {
    int resindex = 0;
    while( resindex < MAX_RESOLUTION_SIZE && model->jpeg_resolutions[resindex] > megapixel ) {
	++resindex;
    }
    return resindex < MAX_RESOLUTION_SIZE ? resindex : MAX_RESOLUTION_SIZE-1;
}

int pslr_set_jpeg_resolution(pslr_handle_t h, int megapixel) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int hwres = _get_hw_jpeg_resolution( p->model, megapixel );
    return ipslr_handle_command_x18( p, true, 0x14, 2, 1, hwres, 0);
}

int pslr_set_jpeg_image_mode(pslr_handle_t h, pslr_jpeg_image_mode_t image_mode) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (image_mode < 0 || image_mode > PSLR_JPEG_IMAGE_MODE_MAX) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, true, 0x1b, 1, image_mode, 0, 0);
}

int pslr_set_jpeg_sharpness(pslr_handle_t h, int32_t sharpness) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int hw_sharpness = sharpness + (pslr_get_model_jpeg_property_levels( h )-1) / 2;
    if (hw_sharpness < 0 || hw_sharpness >=  p->model->jpeg_property_levels) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, false, 0x21, 2, 0, hw_sharpness, 0);
}

int pslr_set_jpeg_contrast(pslr_handle_t h, int32_t contrast) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int hw_contrast = contrast + (pslr_get_model_jpeg_property_levels( h )-1) / 2;
    if (hw_contrast < 0 || hw_contrast >=  p->model->jpeg_property_levels) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, false, 0x22, 2, 0, hw_contrast, 0);
}

int pslr_set_jpeg_hue(pslr_handle_t h, int32_t hue) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int hw_hue = hue + (pslr_get_model_jpeg_property_levels( h )-1) / 2;
    if (hw_hue < 0 || hw_hue >= p->model->jpeg_property_levels) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, false, 0x25, 2, 0, hw_hue, 0);
}

int pslr_set_jpeg_saturation(pslr_handle_t h, int32_t saturation) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int hw_saturation = saturation + (pslr_get_model_jpeg_property_levels( h )-1) / 2;
    if (hw_saturation < 0 || hw_saturation >=  p->model->jpeg_property_levels) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, false, 0x20, 2, 0, hw_saturation, 0);
}

int pslr_set_image_format(pslr_handle_t h, pslr_image_format_t format) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (format < 0 || format > PSLR_IMAGE_FORMAT_MAX) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, true, 0x12, 2, 1, format, 0);
}

int pslr_set_raw_format(pslr_handle_t h, pslr_raw_format_t format) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (format < 0 || format > PSLR_RAW_FORMAT_MAX) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, true, 0x1f, 2, 1, format, 0);
}

int pslr_set_color_space(pslr_handle_t h, pslr_color_space_t color_space) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (color_space < 0 || color_space > PSLR_COLOR_SPACE_MAX) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, true, 0x23, 1, color_space, 0, 0);
}


int pslr_delete_buffer(pslr_handle_t h, int bufno) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (bufno < 0 || bufno > 9)
        return PSLR_PARAM;
    CHECK(ipslr_write_args(p, 1, bufno));
    CHECK(command(p->fd, 0x02, 0x03, 0x04));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

int pslr_green_button(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    CHECK(command(p->fd, 0x10, 0x07, 0x00));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

int pslr_ae_lock(pslr_handle_t h, bool lock) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (lock)
        CHECK(command(p->fd, 0x10, 0x06, 0x00));
    else
        CHECK(command(p->fd, 0x10, 0x08, 0x00));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

int pslr_set_exposure_mode(pslr_handle_t h, pslr_exposure_mode_t mode) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;

    if (mode < 0 || mode >= PSLR_EXPOSURE_MODE_MAX) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, true, 0x01, 2, 1, mode, 0);
}

int pslr_buffer_open(pslr_handle_t h, int bufno, pslr_buffer_type buftype, int bufres) {
    pslr_buffer_segment_info info;
    uint16_t bufs;
    uint32_t buf_total = 0;
    int i, j;
    int ret;
    int retry = 0;
    int retry2 = 0;

    ipslr_handle_t *p = (ipslr_handle_t *) h;

    memset(&info, 0, sizeof (info));

    CHECK(ipslr_status_full(p, &p->status));
    bufs = p->status.bufmask;
    if ((bufs & (1 << bufno)) == 0) {
        DPRINT("No buffer data (%d)\n", bufno);
        return PSLR_READ_ERROR;
    }

    while (retry < 3) {
        /* If we get response 0x82 from the camera, there is a
         * desynch. We can recover by stepping through segment infos
         * until we get the last one (b = 2). Retry up to 3 times. */
        ret = ipslr_select_buffer(p, bufno, buftype, bufres);
        if (ret == PSLR_OK)
            break;

        retry++;
        retry2 = 0;
        /* Try up to 9 times to reach segment info type 2 (last
         * segment) */
        do {
            CHECK(ipslr_buffer_segment_info(p, &info));
            CHECK(ipslr_next_segment(p));
            DPRINT("Recover: b=%d\n", info.b);
        } while (++retry2 < 10 && info.b != 2);
    }

    if (retry == 3)
        return ret;

    i = 0;
    j = 0;
    do {
        CHECK(ipslr_buffer_segment_info(p, &info));
        DPRINT("%d: addr: 0x%x len: %d B=%d\n", i, info.addr, info.length, info.b);
        if (info.b == 4)
            p->segments[j].offset = info.length;
        else if (info.b == 3) {
            if (j == MAX_SEGMENTS) {
                DPRINT("Too many segments.\n");
                return PSLR_NO_MEMORY;
            }
            p->segments[j].addr = info.addr;
            p->segments[j].length = info.length;
            j++;
        }
        CHECK(ipslr_next_segment(p));
        buf_total += info.length;
        i++;
    } while (i < 9 && info.b != 2);
    p->segment_count = j;
    p->offset = 0;
    return PSLR_OK;
}

uint32_t pslr_buffer_read(pslr_handle_t h, uint8_t *buf, uint32_t size) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int i;
    uint32_t pos = 0;
    uint32_t seg_offs;
    uint32_t addr;
    uint32_t blksz;
    int ret;

    /* Find current segment */
    for (i = 0; i < p->segment_count; i++) {
        if (p->offset < pos + p->segments[i].length)
            break;
        pos += p->segments[i].length;
    }

    seg_offs = p->offset - pos;
    addr = p->segments[i].addr + seg_offs;

    /* Compute block size */
    blksz = size;
    if (blksz > p->segments[i].length - seg_offs)
        blksz = p->segments[i].length - seg_offs;
    if (blksz > BLKSZ)
        blksz = BLKSZ;

    DPRINT("File offset %d segment: %d offset %d address 0x%x read size %d\n", p->offset, 
           i, seg_offs, addr, blksz);

    ret = ipslr_download(p, addr, blksz, buf);
    if (ret != PSLR_OK)
        return 0;
    p->offset += blksz;
    return blksz;
}

uint32_t pslr_buffer_get_size(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int i;
    uint32_t len = 0;
    for (i = 0; i < p->segment_count; i++) {
        len += p->segments[i].length;
    }
    return len;
}

void pslr_buffer_close(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    memset(&p->segments[0], 0, sizeof (p->segments));
    p->offset = 0;
    p->segment_count = 0;
}

int pslr_select_af_point(pslr_handle_t h, uint32_t point) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, 0x07, 1, point, 0, 0);
}

int pslr_get_model_jpeg_stars(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->jpeg_stars;
}

int pslr_get_model_buffer_size(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->buffer_size;
}

int pslr_get_model_jpeg_property_levels(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->jpeg_property_levels;
}

int *pslr_get_model_jpeg_resolutions(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->jpeg_resolutions;
}

int pslr_get_model_fastest_shutter_speed(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->fastest_shutter_speed;
}


const char *pslr_camera_name(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int ret;
    if (p->id1 == 0) {
        ret = ipslr_identify(p);
        if (ret != PSLR_OK)
            return NULL;
    }
    if (p->model)
        return p->model->name;
    else {
        static char unk_name[256];
        snprintf(unk_name, sizeof (unk_name), "ID#%x", p->id1);
        unk_name[sizeof (unk_name) - 1] = '\0';
        return unk_name;
    }
}

pslr_buffer_type pslr_get_jpeg_buffer_type(pslr_handle_t h, int quality) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return 2 + _get_hw_jpeg_quality( p->model, quality );
}

/* ----------------------------------------------------------------------- */

static int ipslr_set_mode(ipslr_handle_t *p, uint32_t mode) {
    CHECK(ipslr_write_args(p, 1, mode));
    CHECK(command(p->fd, 0, 0, 4));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

static int ipslr_cmd_00_09(ipslr_handle_t *p, uint32_t mode) {
    CHECK(ipslr_write_args(p, 1, mode));
    CHECK(command(p->fd, 0, 9, 4));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

static int ipslr_cmd_10_0a(ipslr_handle_t *p, uint32_t mode) {
    CHECK(ipslr_write_args(p, 1, mode));
    CHECK(command(p->fd, 0x10, 0x0a, 4));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

static int ipslr_cmd_00_05(ipslr_handle_t *p) {
    int n;
    uint8_t buf[0xb8];
    CHECK(command(p->fd, 0x00, 0x05, 0x00));
    n = get_result(p->fd);
    if (n != 0xb8) {
        DPRINT("only got %d bytes\n", n);
        return PSLR_READ_ERROR;
    }
    CHECK(read_result(p->fd, buf, n));
    return PSLR_OK;
}

static int ipslr_status(ipslr_handle_t *p, uint8_t *buf) {
    int n;
    CHECK(command(p->fd, 0, 1, 0));
    n = get_result(p->fd);
    if (n == 16 || n == 28) {
        return read_result(p->fd, buf, n);
    } else {
        return PSLR_READ_ERROR;
    }
}

#ifdef DEBUG
static uint8_t lastbuf[MAX_STATUS_BUF_SIZE];
static int first = 1;

static void ipslr_status_diff(uint8_t *buf) {
    int n;
    int diffs;
    if (first) {
        hexdump(buf, MAX_STATUS_BUF_SIZE);
        memcpy(lastbuf, buf, MAX_STATUS_BUF_SIZE);
        first = 0;
    }

    diffs = 0;
    for (n = 0; n < MAX_STATUS_BUF_SIZE; n++) {
        if (lastbuf[n] != buf[n]) {
            DPRINT("buf[%03X] last %02Xh %3d new %02Xh %3d\n", n, lastbuf[n], lastbuf[n], buf[n], buf[n]);
            diffs++;
        }
    }
    if (diffs) {
        DPRINT("---------------------------\n");
        memcpy(lastbuf, buf, MAX_STATUS_BUF_SIZE);
    }
}
#endif

int ipslr_status_parse_k10d(ipslr_handle_t *p, pslr_status *status, int n) {
        /* K10D status block */
    uint8_t *buf = p->status_buffer;
        memset(status, 0, sizeof (*status));
        status->bufmask = buf[0x16] << 8 | buf[0x17];
        status->current_iso = get_uint32(&buf[0x11c]);
        status->current_shutter_speed.nom = get_uint32(&buf[0xf4]);
        status->current_shutter_speed.denom = get_uint32(&buf[0xf8]);
        status->current_aperture.nom = get_uint32(&buf[0xfc]);
        status->current_aperture.denom = get_uint32(&buf[0x100]);
        status->lens_min_aperture.nom = get_uint32(&buf[0x12c]);
        status->lens_min_aperture.denom = get_uint32(&buf[0x130]);
        status->lens_max_aperture.nom = get_uint32(&buf[0x134]);
        status->lens_max_aperture.denom = get_uint32(&buf[0x138]);
        status->set_aperture.nom = get_uint32(&buf[0x34]);
        status->set_aperture.denom = get_uint32(&buf[0x38]);
        status->set_shutter_speed.nom = get_uint32(&buf[0x2c]);
        status->set_shutter_speed.denom = get_uint32(&buf[0x30]);
        status->fixed_iso = get_uint32(&buf[0x60]);
        status->jpeg_resolution = get_uint32(&buf[0x7c]);
        status->jpeg_contrast = get_uint32(&buf[0x94]);
        status->jpeg_sharpness = get_uint32(&buf[0x90]);
        status->jpeg_saturation = get_uint32(&buf[0x8c]);
        status->jpeg_quality = _get_user_jpeg_quality( p->model, get_uint32(&buf[0x80]));
        status->jpeg_image_mode = get_uint32(&buf[0x88]);
        status->zoom.nom = get_uint32(&buf[0x16c]);
        status->zoom.denom = get_uint32(&buf[0x170]);
        status->focus = get_int32(&buf[0x174]);
        status->raw_format = get_uint32(&buf[0x84]);
        status->image_format = get_uint32(&buf[0x78]);
        status->light_meter_flags = get_uint32(&buf[0x124]);
        status->ec.nom = get_uint32(&buf[0x3c]);
        status->ec.denom = get_uint32(&buf[0x40]);
        status->custom_ev_steps = get_uint32(&buf[0x9c]);
        status->custom_sensitivity_steps = get_uint32(&buf[0xa0]);
        status->exposure_mode = get_uint32(&buf[0xe0]);
        status->user_mode_flag = get_uint32(&buf[0x1c]);
        status->af_point_select = get_uint32(&buf[0xbc]);
        status->selected_af_point = get_uint32(&buf[0xc0]);
        status->focused_af_point = get_uint32(&buf[0x150]);
        return PSLR_OK;
}

int ipslr_status_parse_k20d(ipslr_handle_t *p, pslr_status *status, int n) {

    uint8_t *buf = p->status_buffer;
#ifdef DEBUG
        ipslr_status_diff(buf);
#endif
        memset(status, 0, sizeof (*status));
        status->bufmask = buf[0x16] << 8 | buf[0x17];
        status->current_iso = get_uint32(&buf[0x130]); //d
        status->current_shutter_speed.nom = get_uint32(&buf[0x108]); //d
        status->current_shutter_speed.denom = get_uint32(&buf[0x10C]); //d
        status->current_aperture.nom = get_uint32(&buf[0x110]); //d
        status->current_aperture.denom = get_uint32(&buf[0x114]); //d
        status->lens_min_aperture.nom = get_uint32(&buf[0x140]); //d
        status->lens_min_aperture.denom = get_uint32(&buf[0x144]); //d
        status->lens_max_aperture.nom = get_uint32(&buf[0x148]); //d
        status->lens_max_aperture.denom = get_uint32(&buf[0x14B]); //d
        status->set_aperture.nom = get_uint32(&buf[0x34]); //d
        status->set_aperture.denom = get_uint32(&buf[0x38]); //d
        status->set_shutter_speed.nom = get_uint32(&buf[0x2c]); //d
        status->set_shutter_speed.denom = get_uint32(&buf[0x30]); //d
        status->fixed_iso = get_uint32(&buf[0x60]); //d
        status->jpeg_resolution = get_uint32(&buf[0x7c]); //d
        status->jpeg_contrast = get_uint32(&buf[0x94]); // commands do now work for it?
        status->jpeg_sharpness = get_uint32(&buf[0x90]); // commands do now work for it?
        status->jpeg_saturation = get_uint32(&buf[0x8c]); // commands do now work for it?
        status->jpeg_quality = _get_user_jpeg_quality( p->model, get_uint32(&buf[0x80])); //d
        status->jpeg_image_mode = get_uint32(&buf[0x88]); //d
        status->zoom.nom = get_uint32(&buf[0x180]); //d
        status->zoom.denom = get_uint32(&buf[0x184]); //d
        status->focus = get_int32(&buf[0x188]); //d current focus ring position?
        status->raw_format = get_uint32(&buf[0x84]); //d
        status->image_format = get_uint32(&buf[0x78]); //d
        status->light_meter_flags = get_uint32(&buf[0x138]); //d
        status->ec.nom = get_uint32(&buf[0x3c]); //d
        status->ec.denom = get_uint32(&buf[0x40]); //d
        status->custom_ev_steps = get_uint32(&buf[0x9c]);
        status->custom_sensitivity_steps = get_uint32(&buf[0xa0]);
        status->exposure_mode = get_uint32(&buf[0xe0]); //d
        status->user_mode_flag = get_uint32(&buf[0x1c]); //d
        status->af_point_select = get_uint32(&buf[0xbc]); // not sure
        status->selected_af_point = get_uint32(&buf[0xc0]); //d
        status->focused_af_point = get_uint32(&buf[0x160]); //d, unsure about it, a lot is changing when the camera focuses
        // 0x158 current ev?
        // 0xB8 0 - MF, 1 - AF.S, 2 - AF.C
        // 0xB4, 0xC4 - metering mode, 0 - matrix, 1 - center weighted, 2 - spot
        // 0x160 and 0x164 change when AF
        // 0xC0 changes when selecting focus point manually or from GUI
        // 0xBC focus point selection 0 - auto, 1 - manual, 2 - center
        return PSLR_OK;

}

int ipslr_status_parse_istds(ipslr_handle_t *p, pslr_status *status, int n) {

    uint8_t *buf = p->status_buffer;
        /* *ist DS status block */
        memset(status, 0, sizeof (*status));
        status->bufmask = get_uint32(&buf[0x10]);
        status->set_shutter_speed.nom = get_uint32(&buf[0x80]);
        status->set_shutter_speed.denom = get_uint32(&buf[0x84]);
        status->set_aperture.nom = get_uint32(&buf[0x88]);
        status->set_aperture.denom = get_uint32(&buf[0x8c]);
        status->lens_min_aperture.nom = get_uint32(&buf[0xb8]);
        status->lens_min_aperture.denom = get_uint32(&buf[0xbc]);
        status->lens_max_aperture.nom = get_uint32(&buf[0xc0]);
        status->lens_max_aperture.denom = get_uint32(&buf[0xc4]);

        return PSLR_OK;
}

int ipslr_status_parse_kx(ipslr_handle_t *p, pslr_status *status, int n) {

    uint8_t *buf = p->status_buffer;
        /* K-x status block */
#ifdef DEBUG
        ipslr_status_diff(buf);
#endif

        memset(status, 0, sizeof (*status));	
        status->bufmask = buf[0x1E] << 8 | buf[0x1F];

        status->current_iso = get_uint32(&buf[0x134]); //d

        status->current_shutter_speed.nom = get_uint32(&buf[0x10C]); //d
        status->current_shutter_speed.denom = get_uint32(&buf[0x110]); //d
        status->current_aperture.nom = get_uint32(&buf[0x114]); //d 3C
        status->current_aperture.denom = get_uint32(&buf[0x118]); //d 40
        status->lens_min_aperture.nom = get_uint32(&buf[0x144]); //d
        status->lens_min_aperture.denom = get_uint32(&buf[0x148]); //d
        status->lens_max_aperture.nom = get_uint32(&buf[0x14C]); //d
        status->lens_max_aperture.denom = get_uint32(&buf[0x150]); //d
        status->set_aperture.nom = get_uint32(&buf[0x3C]); //d
        status->set_aperture.denom = get_uint32(&buf[0x40]); //d
        status->set_shutter_speed.nom = get_uint32(&buf[0x34]); //d
        status->set_shutter_speed.denom = get_uint32(&buf[0x38]); //d
        status->fixed_iso = get_uint32(&buf[0x68]); //d
        status->jpeg_resolution = get_uint32(&buf[0x84]); //d
        status->jpeg_contrast = get_uint32(&buf[0x9C]); // commands do now work for it?
        status->jpeg_sharpness = get_uint32(&buf[0x98]); // commands do now work for it?
        status->jpeg_saturation = get_uint32(&buf[0x94]); // commands do now work for it?
        status->jpeg_hue = get_uint32(&buf[0xFC]);
        status->jpeg_quality = _get_user_jpeg_quality( p->model, get_uint32(&buf[0x88])); //d
        status->jpeg_image_mode = get_uint32(&buf[0x90]); //d
        status->zoom.nom = get_uint32(&buf[0x198]); //d
        status->zoom.denom = get_uint32(&buf[0x19C]); //d
        status->focus = get_int32(&buf[0x1A0]); //d
        status->raw_format = get_uint32(&buf[0x8C]); //d
        status->image_format = get_uint32(&buf[0x80]); //d
        status->light_meter_flags = get_uint32(&buf[0x140]); //d
        status->ec.nom = get_uint32(&buf[0x44]); //d
        status->ec.denom = get_uint32(&buf[0x48]); //d
        status->custom_ev_steps = get_uint32(&buf[0xA4]);
        status->custom_sensitivity_steps = get_uint32(&buf[0xa8]);
        status->exposure_mode = get_uint32(&buf[0xb4]); //d
        status->user_mode_flag = get_uint32(&buf[0x24]); //d
        status->af_mode = get_uint32(&buf[0xC0]);
        status->af_point_select = get_uint32(&buf[0xc4]);
        status->selected_af_point = get_uint32(&buf[0xc8]); //d
        status->focused_af_point = get_uint32(&buf[0x168]); //d, unsure about it, a lot is changing when the camera focuses
        status->auto_iso_min = get_uint32(&buf[0x6C]);
        status->auto_iso_max = get_uint32(&buf[0x70]);
        status->drive_mode = get_uint32(&buf[0x5C]);
        status->bracket_ev = get_uint32(&buf[0x50]);
        status->shake_reduction = get_uint32(&buf[0xE0]);
        status->white_balance_mode = get_uint32(&buf[0x74]);
        status->white_balance_adjust_mg = get_uint32(&buf[0x78]); // 0: M7 7: 0 14: G7
        status->white_balance_adjust_ba = get_uint32(&buf[0x7C]); // 0: B7 7: 0 14: A7 
        status->flash_mode = get_uint32(&buf[0x28]);
        status->flash_exposure_compensation = get_int32(&buf[0x2C]);
        status->manual_mode_ev = get_int32(&buf[0x15C]);
        status->color_space = get_uint32(&buf[0xA0]);
	// 0x58 bracket picture count ?

        return PSLR_OK;
}

int ipslr_status_parse_k200d(ipslr_handle_t *p, pslr_status *status, int n) {
        
        
    uint8_t *buf = p->status_buffer;
#ifdef DEBUG
        ipslr_status_diff(buf);
#endif

        memset(status, 0, sizeof (*status));	
        status->bufmask = buf[0x16] << 8 | buf[0x17];
        status->current_iso = get_uint32(&buf[0x060]); 
        status->current_shutter_speed.nom = get_uint32(&buf[0x0104]); 
        status->current_shutter_speed.denom = get_uint32(&buf[0x108]); 
        status->current_aperture.nom = get_uint32(&buf[0x034]); 
        status->current_aperture.denom = get_uint32(&buf[0x038]); 
        status->lens_max_aperture.nom = get_uint32(&buf[0x144]); 
        status->lens_max_aperture.denom = get_uint32(&buf[0x148]); 
        status->lens_min_aperture.nom = get_uint32(&buf[0x13c]);
        status->lens_min_aperture.denom = get_uint32(&buf[0x140]);
        status->set_aperture.nom = get_uint32(&buf[0x34]);
        status->set_aperture.denom = get_uint32(&buf[0x38]);
        status->set_shutter_speed.nom = get_uint32(&buf[0x2c]);
        status->set_shutter_speed.denom = get_uint32(&buf[0x30]);
        status->fixed_iso = get_uint32(&buf[0x60]);
        status->jpeg_resolution = get_uint32(&buf[0x7c]);
        status->jpeg_contrast = get_uint32(&buf[0x94]);
	status->jpeg_hue = get_uint32(&buf[0xf4]);
        status->jpeg_sharpness = get_uint32(&buf[0x90]);
        status->jpeg_saturation = get_uint32(&buf[0x8c]);
        status->jpeg_quality = _get_user_jpeg_quality( p->model, get_uint32(&buf[0x80]));
        status->jpeg_image_mode = get_uint32(&buf[0x88]);
        status->zoom.nom = get_uint32(&buf[0x17c]);
        status->zoom.denom = get_uint32(&buf[0x180]);
        status->auto_iso_min = get_uint32(&buf[0x64]);
        status->auto_iso_max = get_uint32(&buf[0x68]);
        status->focus = get_int32(&buf[0x184]);
        status->raw_format = get_uint32(&buf[0x84]);
        status->image_format = get_uint32(&buf[0x78]);
        status->light_meter_flags = get_uint32(&buf[0x124]);
        status->ec.nom = get_uint32(&buf[0x3c]);
        status->ec.denom = get_uint32(&buf[0x40]);
        //status->custom_ev_steps = get_uint32(&buf[0x9c]);
        //status->custom_sensitivity_steps = get_uint32(&buf[0xa0]);
        status->exposure_mode = get_uint32(&buf[0xac]);
	status->user_mode_flag = get_uint32(&buf[0x1c]);
        status->af_point_select = get_uint32(&buf[0xbc]);
	status->af_mode = get_uint32(&buf[0xb8]);
        status->selected_af_point = get_uint32(&buf[0xc0]);
        status->focused_af_point = get_uint32(&buf[0x150]);
	status->shake_reduction = get_uint32(&buf[0xda]);
        // Drive mode: 0=Single shot, 1= Continous Hi, 2= Continous Low or Self timer 12s, 3=Self timer 2s
	// 4= remote, 5= remote 3s delay
	status->drive_mode = get_uint32(&buf[0xcc]); 
        return PSLR_OK;
}

static int ipslr_status_full(ipslr_handle_t *p, pslr_status *status) {
    int n;
    CHECK(command(p->fd, 0, 8, 0));
    n = get_result(p->fd);
    int expected_bufsize = p->model->buffer_size;

    CHECK(read_result(p->fd, p->status_buffer, n > MAX_STATUS_BUF_SIZE ? MAX_STATUS_BUF_SIZE: n));

    if( p->model && expected_bufsize > 0 && expected_bufsize != n ) {
	DPRINT("Waiting for %d bytes but got %d\n", expected_bufsize, n);
        return PSLR_READ_ERROR;
//	return PSLR_OK;
    }

    int ret;
    if( p->model && p->model->parser_function ) {
	ret = (*p->model->parser_function)(p, status, n);
    } else {
        /* Unknown camera */
	ret = PSLR_OK;
    }

    // required for K-x, probably for other cameras too
    status->exposure_mode = exposure_mode_conversion( status->exposure_mode );

    return ret;
}

static int ipslr_press_shutter(ipslr_handle_t *p) {
    int r;
    uint32_t bufmask;
    CHECK(ipslr_status_full(p, &p->status));
    bufmask = p->status.bufmask;
    DPRINT("before: mask=0x%x\n", p->status.bufmask);
    CHECK(ipslr_write_args(p, 1, 2));
    CHECK(command(p->fd, 0x10, 0x05, 0x04));
    r = get_status(p->fd);
    DPRINT("shutter result code: 0x%x\n", r);
    return PSLR_OK;
}

/* static int ipslr_read_buffer(ipslr_handle_t *p, int bufno, pslr_buffer_type buftype, int bufres, */
/*         uint8_t **ppData, uint32_t *pLen) { */
/*     pslr_buffer_segment_info info[9]; */
/*     uint16_t bufs; */
/*     uint32_t bufaddr; */
/*     uint32_t buflen = 0; */
/*     uint32_t buf_total = 0; */
/*     int i; */
/*     uint8_t *buf = 0; */
/*     uint8_t *buf_ptr; */
/*     int result; */
/*     int num_info; */
/*     int ret; */
/*     int retry = 0; */
/*     int retry2 = 0; */

/*     memset(&info, 0, sizeof (info)); */

/*     CHECK(ipslr_status_full(p, &p->status)); */
/*     bufs = p->status.bufmask; */
/*     if ((bufs & (1 << bufno)) == 0) { */
/*         DPRINT("No buffer data (%d)\n", bufno); */
/*         return PSLR_OK; */
/*     } */

/*     while (retry < 3) { */
/*         /\* If we get response 0x82 from the camera, there is a */
/*          * desynch. We can recover by stepping through segment infos */
/*          * until we get the last one (b = 2). Retry up to 3 times. *\/ */
/*         ret = ipslr_select_buffer(p, bufno, buftype, bufres); */
/*         if (ret == PSLR_OK) */
/*             break; */

/*         retry++; */
/*         retry2 = 0; */
/*         /\* Try up to 9 times to reach segment info type 2 (last */
/*          * segment) *\/ */
/*         do { */
/*             CHECK(ipslr_buffer_segment_info(p, &info[0])); */
/*             CHECK(ipslr_next_segment(p)); */
/*             DPRINT("Recover: b=%d\n", info[0].b); */
/*         } while (++retry2 < 10 && info[0].b != 2); */
/*     } */

/*     if (retry == 3) */
/*         return ret; */

/*     i = 0; */
/*     do { */
/*         CHECK(ipslr_buffer_segment_info(p, &info[i])); */
/*         DPRINT("%d: addr: 0x%x len: %d B=%d\n", i, info[i].addr, info[i].length, info[i].b); */
/*         CHECK(ipslr_next_segment(p)); */
/*         buf_total += info[i].length; */
/*         i++; */
/*     } while (i < 9 && info[i - 1].b != 2); */
/*     num_info = i; */
/*     buf = malloc(buf_total); */
/*     if (!buf) */
/*         return PSLR_NO_MEMORY; */
/*     buf_ptr = buf; */
/*     for (i = 0; i < num_info; i++) { */
/*         bufaddr = info[i].addr; */
/*         buflen = info[i].length; */
/*         if (bufaddr && buflen) { */
/*             result = ipslr_download(p, bufaddr, buflen, buf_ptr); */
/*             if (result != PSLR_OK) { */
/*                 free(buf); */
/*                 return result; */
/*             } */
/*             buf_ptr += buflen; */
/*         } else { */
/*             printf("empty segment\n"); */
/*         } */
/*     } */

/*     if (ppData) */
/*         *ppData = buf; */
/*     if (pLen) */
/*         *pLen = buf_total; */
/*     return PSLR_OK; */
/* } */

static int ipslr_select_buffer(ipslr_handle_t *p, int bufno, pslr_buffer_type buftype, int bufres) {
    int r;
    DPRINT("Select buffer %d,%d,%d,0\n", bufno, buftype, bufres);
    if( !p->model->old_scsi_command ) {
        CHECK(ipslr_write_args(p, 4, bufno, buftype, bufres, 0));
        CHECK(command(p->fd, 0x02, 0x01, 0x10));
    } else {
        /* older cameras: 3-arg select buffer */
        CHECK(ipslr_write_args(p, 4, bufno, buftype, bufres));
        CHECK(command(p->fd, 0x02, 0x01, 0x0c));
    }
    r = get_status(p->fd);
    if (r != 0) {
        return PSLR_COMMAND_ERROR;
    }
    return PSLR_OK;
}

static int ipslr_next_segment(ipslr_handle_t *p) {
    int r;
    CHECK(ipslr_write_args(p, 1, 0));
    CHECK(command(p->fd, 0x04, 0x01, 0x04));
    usleep(100000); // needed !! 100 too short, 1000 not short enough for PEF
    r = get_status(p->fd);
    if (r == 0)
        return PSLR_OK;
    return PSLR_COMMAND_ERROR;
}

static int ipslr_buffer_segment_info(ipslr_handle_t *p, pslr_buffer_segment_info *pInfo) {
    uint8_t buf[16];
    uint32_t n;

    CHECK(command(p->fd, 0x04, 0x00, 0x00));
    n = get_result(p->fd);
    if (n != 16)
        return PSLR_READ_ERROR;
    CHECK(read_result(p->fd, buf, 16));
    pInfo->a = get_uint32(&buf[0]);
    pInfo->b = get_uint32(&buf[4]);
    pInfo->addr = get_uint32(&buf[8]);
    pInfo->length = get_uint32(&buf[12]);
    return PSLR_OK;
}

static int ipslr_download(ipslr_handle_t *p, uint32_t addr, uint32_t length, uint8_t *buf) {
    uint8_t downloadCmd[8] = {0xf0, 0x24, 0x06, 0x02, 0x00, 0x00, 0x00, 0x00};
    uint32_t block;
    int n;
    int retry;
    int r;
    uint32_t length_start = length;

    retry = 0;
    while (length > 0) {
        if (length > BLKSZ) {
            block = BLKSZ;
        } else {
            block = length;
	}

        //DPRINT("Get 0x%x bytes from 0x%x\n", block, addr);
        CHECK(ipslr_write_args(p, 2, addr, block));
        CHECK(command(p->fd, 0x06, 0x00, 0x08));
        r = get_status(p->fd);

        n = scsi_read(p->fd, downloadCmd, sizeof (downloadCmd), buf, block);
        r = get_status(p->fd);

        if (n < 0) {
            if (retry < BLOCK_RETRY) {
                retry++;
                continue;
            }
            return PSLR_READ_ERROR;
        }
        buf += n;
        length -= n;
        addr += n;
        retry = 0;
        if (progress_callback) {
            progress_callback(length_start - length, length_start);
        }
    }
    return PSLR_OK;
}

static int ipslr_identify(ipslr_handle_t *p) {
    uint8_t idbuf[8];
    int n;
    int i;

    CHECK(command(p->fd, 0, 4, 0));
    n = get_result(p->fd);
    if (n != 8)
        return PSLR_READ_ERROR;
    CHECK(read_result(p->fd, idbuf, 8));
    p->id1 = get_uint32(&idbuf[0]);
    DPRINT("id1 of the camera: %x\n", p->id1);
//    p->id2 = get_uint32(&idbuf[4]);
    p->model = NULL;
    for (i = 0; i<sizeof (camera_models) / sizeof (camera_models[0]); i++) {
        if (camera_models[i].id1 == p->id1) {
            p->model = &camera_models[i];
            break;
        }
    }
    return PSLR_OK;
}

static int ipslr_write_args(ipslr_handle_t *p, int n, ...) {
    va_list ap;
    uint8_t cmd[8] = {0xf0, 0x4f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t buf[4 * n];
    int fd = p->fd;
    int res;
    int i;
    uint32_t data;

    va_start(ap, n);
    if( p->model && !p->model->old_scsi_command ) {
        /* All at once */
        for (i = 0; i < n; i++) {
            data = va_arg(ap, uint32_t);
            buf[4 * i + 0] = data >> 24;
            buf[4 * i + 1] = data >> 16;
            buf[4 * i + 2] = data >> 8;
            buf[4 * i + 3] = data;
        }
        cmd[4] = 4 * n;
        res = scsi_write(fd, cmd, sizeof (cmd), buf, 4 * n);
        if (res != PSLR_OK) {
            return res;
	}
    } else {
        /* Arguments one by one */
        for (i = 0; i < n; i++) {
            data = va_arg(ap, uint32_t);
            buf[0] = data >> 24;
            buf[1] = data >> 16;
            buf[2] = data >> 8;
            buf[3] = data;
            cmd[4] = 4;
            cmd[2] = i * 4;
            res = scsi_write(fd, cmd, sizeof (cmd), buf, 4);
            if (res != PSLR_OK) {
                return res;
	    }
        }
    }
    va_end(ap);
    return PSLR_OK;
}

/* ----------------------------------------------------------------------- */

static int command(int fd, int a, int b, int c) {
    uint8_t cmd[8] = {0xf0, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    cmd[2] = a;
    cmd[3] = b;
    cmd[4] = c;
    CHECK(scsi_write(fd, cmd, sizeof (cmd), 0, 0));
    return PSLR_OK;
}

static int read_status(int fd, uint8_t *buf) {
    uint8_t cmd[8] = {0xf0, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    int n;

    n = scsi_read(fd, cmd, 8, buf, 8);
    if (n != 8) {
        DPRINT("Only got %d bytes\n", n);
        /* The *ist DS doesn't know to return the correct number of
            read bytes for this command, so return PSLR_OK instead of
            PSLR_READ_ERROR */
        return PSLR_OK;
    }
    return PSLR_OK;
}

static int get_status(int fd) {
    uint8_t statusbuf[8];
    while (1) {
        //usleep(POLL_INTERVAL);
        CHECK(read_status(fd, statusbuf));
        //DPRINT("get_status->\n");
        //hexdump(statusbuf, 8);
        if ((statusbuf[7] & 0x01) == 0)
            break;
        //DPRINT("Waiting for ready - ");
        //hexdump(statusbuf, 8);
        usleep(POLL_INTERVAL);
    }
    if ((statusbuf[7] & 0xff) != 0) {
        DPRINT("ERROR: 0x%x\n", statusbuf[7]);
    }
    return statusbuf[7];
}

static int get_result(int fd) {
    uint8_t statusbuf[8];
    while (1) {
        //DPRINT("read out status\n");
        CHECK(read_status(fd, statusbuf));
        //hexdump(statusbuf, 8);
        if (statusbuf[6] == 0x01)
            break;
        //DPRINT("Waiting for result\n");
        //hexdump(statusbuf, 8);
        usleep(POLL_INTERVAL);
    }
    if ((statusbuf[7] & 0xff) != 0) {
        DPRINT("ERROR: 0x%x\n", statusbuf[7]);
        return -1;
    }
    return statusbuf[0] | statusbuf[1] << 8 | statusbuf[2] << 16 | statusbuf[3];
}

static int read_result(int fd, uint8_t *buf, uint32_t n) {
    uint8_t cmd[8] = {0xf0, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    int r;
    cmd[4] = n;
    cmd[5] = n >> 8;
    cmd[6] = n >> 16;
    cmd[7] = n >> 24;
    r = scsi_read(fd, cmd, sizeof (cmd), buf, n);
    if (r != n)
        return PSLR_READ_ERROR;
    return PSLR_OK;
}

void hexdump(uint8_t *buf, uint32_t bufLen) {
    int i;
    for (i = 0; i < bufLen; i++) {
        if (i % 16 == 0) {
            printf("0x%04x | ", i);
	}
        printf("%02x ", buf[i]);
        if (i % 8 == 7) {
            printf(" ");
	}
        if (i % 16 == 15) {
            printf("\n");
	}
    }
    if (i % 16 != 15) {
        printf("\n");
    }
}

/* ----------------------------------------------------------------------- */

static uint32_t get_uint32(uint8_t *buf) {
    uint32_t res;
    res = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
    return res;
}

static int32_t get_int32(uint8_t *buf) {
    int32_t res;
    res = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
    return res;
}
