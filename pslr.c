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
#include "pslr_lens.h"

#define MAX_SEGMENTS 4
#define POLL_INTERVAL 100000 /* Number of us to wait when polling */
#define BLKSZ 65536 /* Block size for downloads; if too big, we get
                     * memory allocation error from sg driver */
#define BLOCK_RETRY 3 /* Number of retries, since we can occasionally
                       * get SCSI errors when downloading data */

#define CHECK(x) do {                           \
        int __r;                                \
        __r = (x);                                                      \
        if (__r != PSLR_OK) {                                           \
            fprintf(stderr, "%s:%d:%s failed: %d\n", __FILE__, __LINE__, #x, __r); \
            return __r;                                                 \
        }                                                               \
    } while (0)

typedef struct {
    uint32_t offset;
    uint32_t addr;
    uint32_t length;
} ipslr_segment_t;

typedef struct ipslr_handle ipslr_handle_t;

typedef int (*ipslr_status_parse_t)(ipslr_handle_t *p, pslr_status *status);

typedef struct {
    uint32_t id1;                                    // Pentax model ID
    const char *name;                                // name
    bool old_scsi_command;                           // 1 for *ist cameras, 0 for the newer cameras
    int buffer_size;                                 // buffer size in bytes
    int jpeg_stars;                                  // maximum jpeg stars
    int jpeg_resolutions[MAX_RESOLUTION_SIZE];       // jpeg resolution table
    int jpeg_property_levels;                        // 5 [-2, 2] or 7 [-3,3] or 9 [-4,4]
    int fastest_shutter_speed;                       // fastest shutter speed denominator
    int base_iso_min;                                // base iso minimum
    int base_iso_max;                                // base iso maximum
    int extended_iso_min;                            // extended iso minimum
    int extended_iso_max;                            // extended iso maximum
    pslr_jpeg_image_tone_t max_supported_image_tone; // last supported jpeg image tone
    ipslr_status_parse_t parser_function;            // parse function for status buffer
} ipslr_model_info_t;

struct ipslr_handle {
    int fd;
    pslr_status status;
    uint32_t id1;
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
static int ipslr_press_shutter(ipslr_handle_t *p, bool fullpress);
static int ipslr_select_buffer(ipslr_handle_t *p, int bufno, pslr_buffer_type buftype, int bufres);
static int ipslr_buffer_segment_info(ipslr_handle_t *p, pslr_buffer_segment_info *pInfo);
static int ipslr_next_segment(ipslr_handle_t *p);
static int ipslr_download(ipslr_handle_t *p, uint32_t addr, uint32_t length, uint8_t *buf);
static int ipslr_identify(ipslr_handle_t *p);
static int ipslr_write_args(ipslr_handle_t *p, int n, ...);

static int command(int fd, int a, int b, int c);
static int get_status(int fd);
static int get_result(int fd);
static int read_result(int fd, uint8_t *buf, uint32_t n);

void hexdump(uint8_t *buf, uint32_t bufLen);

static uint16_t get_uint16(uint8_t *buf);
static uint32_t get_uint32(uint8_t *buf);
static int32_t get_int32(uint8_t *buf);

static pslr_progress_callback_t progress_callback = NULL;

user_file_format_t file_formats[3] = {
    { USER_FILE_FORMAT_PEF, "PEF", "pef"},
    { USER_FILE_FORMAT_DNG, "DNG", "dng"},
    { USER_FILE_FORMAT_JPEG, "JPEG", "jpg"},
};

int ipslr_status_parse_kx(ipslr_handle_t *p, pslr_status *status);
int ipslr_status_parse_kr(ipslr_handle_t *p, pslr_status *status);
int ipslr_status_parse_k20d(ipslr_handle_t *p, pslr_status *status);
int ipslr_status_parse_k10d(ipslr_handle_t *p, pslr_status *status);
int ipslr_status_parse_k200d(ipslr_handle_t *p, pslr_status *status);
int ipslr_status_parse_istds(ipslr_handle_t *p, pslr_status *status);
int ipslr_status_parse_k5(ipslr_handle_t *p, pslr_status *status);
int ipslr_status_parse_km(ipslr_handle_t *p, pslr_status *status);

static ipslr_model_info_t camera_models[] = {
    { 0x12aa2, "*ist DS",  1, 264, 3, {6, 4, 2},      5, 4000, 200, 3200, 200,  3200, PSLR_JPEG_IMAGE_TONE_BRIGHT,        ipslr_status_parse_istds },
    { 0x12cd2, "K20D",     0, 412, 4, {14, 10, 6, 2}, 7, 4000, 100, 3200, 100,  6400, PSLR_JPEG_IMAGE_TONE_MONOCHROME,    ipslr_status_parse_k20d  },
    { 0x12c1e, "K10D",     0, 392, 3, {10, 6, 2},     7, 4000, 100, 1600, 100,  1600, PSLR_JPEG_IMAGE_TONE_BRIGHT,        ipslr_status_parse_k10d  },
    { 0x12c20, "GX10",     0, 392, 3, {10, 6, 2},     7, 4000, 100, 1600, 100,  1600, PSLR_JPEG_IMAGE_TONE_BRIGHT,        ipslr_status_parse_k10d  },
    { 0x12cd4, "GX20",     0, 412, 4, {14, 10, 6, 2}, 7, 4000, 100, 3200, 100,  6400, PSLR_JPEG_IMAGE_TONE_MONOCHROME,    ipslr_status_parse_k20d  },
    { 0x12dfe, "K-x",      0, 436, 3, {12, 10, 6, 2}, 9, 6000, 200, 6400, 100, 12800, PSLR_JPEG_IMAGE_TONE_MUTED,         ipslr_status_parse_kx    },
    { 0x12cfa, "K200D",    0, 408, 3, {10, 6, 2},     9, 4000, 100, 1600, 100,  1600, PSLR_JPEG_IMAGE_TONE_MONOCHROME,    ipslr_status_parse_k200d }, 
    { 0x12db8, "K-7",      0, 436, 4, {14, 10, 6, 2}, 9, 8000, 100, 3200, 100,  6400, PSLR_JPEG_IMAGE_TONE_MUTED,         ipslr_status_parse_kx    },
    { 0x12e6c, "K-r",      0, 440, 3, {12, 10, 6, 2}, 9, 6000, 200,12800, 100, 25600, PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS, ipslr_status_parse_kr   },
    { 0x12e76, "K-5",      0, 444, 4, {16, 10, 6, 2}, 9, 8000, 100,12800,  80, 51200, PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS, ipslr_status_parse_k5   },
    { 0x12d72, "K-2000",   0, 412, 3, {10, 6, 2},     9, 4000, 100, 3200, 100,  3200, PSLR_JPEG_IMAGE_TONE_MONOCHROME,    ipslr_status_parse_km    },
    { 0x12d73, "K-m",      0, 412, 3, {10, 6, 2},     9, 4000, 100, 3200, 100,  3200, PSLR_JPEG_IMAGE_TONE_MONOCHROME,    ipslr_status_parse_km    },
// only limited support from here
    { 0x12994, "*ist D",   1, 0,   3, {6, 4, 2}, 3, 4000, 200, 3200, 200, 3200, -1, NULL},
    { 0x12b60, "*ist DS2", 1, 0,   3, {6, 4, 2}, 5, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_BRIGHT, NULL},
    { 0x12b1a, "*ist DL",  1, 0,   3, {6, 4, 2}, 5, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_BRIGHT, NULL},
    { 0x12b9d, "K110D",    0, 0,   3, {6, 4, 2}, 5, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_BRIGHT, NULL},
    { 0x12b9c, "K100D",    0, 0,   3, {6, 4, 2}, 5, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_BRIGHT, NULL},
};

const char* valid_vendors[2] = {"PENTAX", "SAMSUNG"};
const char* valid_models[2] = {"DIGITAL_CAMERA", "DSC"}; // no longer list all of them, DSC* should be ok

// x18 cubcommands to change camera properties
// X18_n: unknown effect
typedef enum {
    X18_00,
    X18_EXPOSURE_MODE,
    X18_02,
    X18_AE_METERING_MODE,
    X18_FLASH_MODE,
    X18_AF_MODE,
    X18_AF_POINT_SEL,
    X18_AF_POINT,
    X18_08,
    X18_09,
    X18_0A,
    X18_0B,
    X18_0C,
    X18_0D,
    X18_0E,
    X18_0F,
    X18_WHITE_BALANCE,
    X18_11, // also white balance, but only adjustments
    X18_IMAGE_FORMAT,
    X18_JPEG_STARS,
    X18_JPEG_RESOLUTION,
    X18_ISO,
    X18_SHUTTER,
    X18_APERTURE,
    X18_EC,
    X18_19,
    X18_FLASH_EXPOSURE_COMPENSATION,
    X18_JPEG_IMAGE_TONE,
    X18_DRIVE_MODE,
    X18_1D,
    X18_1E,
    X18_RAW_FORMAT,
    X18_JPEG_SATURATION,
    X18_JPEG_SHARPNESS,
    X18_JPEG_CONTRAST,
    X18_COLOR_SPACE,
    X18_24,
    X18_JPEG_HUE
} x18_subcommands_t;


user_file_format_t *get_file_format_t( user_file_format uff ) {
    int i;    
    for (i = 0; i<sizeof(file_formats) / sizeof(file_formats[0]); i++) {
        if (file_formats[i].uff == uff) {
            return &file_formats[i];
        }
    }
    return NULL;
}

int pslr_set_user_file_format(pslr_handle_t h, user_file_format uff) {
    switch( uff ) {
    case USER_FILE_FORMAT_PEF:
	pslr_set_image_format(h, PSLR_IMAGE_FORMAT_RAW);
	pslr_set_raw_format(h, PSLR_RAW_FORMAT_PEF);
	break;
    case USER_FILE_FORMAT_DNG:
	pslr_set_image_format(h, PSLR_IMAGE_FORMAT_RAW);
	pslr_set_raw_format(h, PSLR_RAW_FORMAT_DNG);
	break;
    case USER_FILE_FORMAT_JPEG:
	pslr_set_image_format(h, PSLR_IMAGE_FORMAT_JPEG);
	break;
    case USER_FILE_FORMAT_MAX:
	return PSLR_PARAM;
    }
    return PSLR_OK;
}

user_file_format get_user_file_format( pslr_status *st ) {
    int rawfmt = st->raw_format;
    int imgfmt = st->image_format;
    if (imgfmt == PSLR_IMAGE_FORMAT_JPEG) {
        return USER_FILE_FORMAT_JPEG;
    } else {
        if (rawfmt == PSLR_RAW_FORMAT_PEF) {
            return USER_FILE_FORMAT_PEF;
        } else {
            return USER_FILE_FORMAT_DNG;
	}
    }
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
	if( find_in_array( valid_vendors, sizeof(valid_vendors)/sizeof(valid_vendors[0]),vendorId) != -1 
	    && find_in_array( valid_models, sizeof(valid_models)/sizeof(valid_models[0]), productId) != -1 ) {
	    if( result == PSLR_OK ) {
		DPRINT("Found camera %s %s\n", vendorId, productId);
		pslr.fd = fd;
		/* Only support first connected camera at this time. */
		return &pslr;	    
	    } else {
		DPRINT("Cannot get drive info of Pentax camera. Please do not forget to install the program using 'make install'\n");
		// found the camera but communication is not possible
		close( fd );
		continue;
	    }
	} else {
	    close( fd );
	    continue;
	}
    }
    DPRINT("camera not found\n");
    return NULL;
}

int pslr_connect(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    uint8_t statusbuf[28];
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
    uint8_t statusbuf[28];
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
    return ipslr_press_shutter(p, true);
}

int pslr_focus(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_press_shutter(p, false);
}

int pslr_get_status(pslr_handle_t h, pslr_status *ps) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    memset( ps, 0, sizeof( pslr_status ));
    CHECK(ipslr_status_full(p, &p->status));
    memcpy(ps, &p->status, sizeof (pslr_status));
    return PSLR_OK;
}

char *format_rational( pslr_rational_t rational, char * fmt ) {
    char *ret = malloc(32);
    if( rational.denom == 0 ) {
	snprintf( ret, 32, "unknown" );
    } else {
	snprintf( ret, 32, fmt, 1.0 * rational.nom / rational.denom );
    }
    return ret;
}

char *get_white_balance_single_adjust_str( uint32_t adjust, char negativeChar, char positiveChar ) {
    char *ret = malloc(4);
    if( adjust < 7 ) {
	snprintf( ret, 4, "%c%d", negativeChar, 7-adjust);
    } else if( adjust > 7 ) {
	snprintf( ret, 4, "%c%d", positiveChar, adjust-7);
    } else {
	ret = "";
    }
    return ret;
}

char *get_white_balance_adjust_str( uint32_t adjust_mg, uint32_t adjust_ba ) {
    char *ret = malloc(8);
    if( adjust_mg != 7 || adjust_ba != 7 ) {
	snprintf(ret, 8, "%s%s", get_white_balance_single_adjust_str(adjust_mg, 'M', 'G'),get_white_balance_single_adjust_str(adjust_ba, 'B', 'A')); } else {
	ret = "0";
    }
    return ret;
}  

char *collect_status_info( pslr_handle_t h, pslr_status status ) {    
    char *strbuffer = malloc(8192);
    sprintf(strbuffer,"%-32s: %d\n", "current iso", status.current_iso);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d/%d\n", "current shutter speed", status.current_shutter_speed.nom, status.current_shutter_speed.denom);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d/%d\n", "camera max shutter speed", status.max_shutter_speed.nom, status.max_shutter_speed.denom);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "current aperture", format_rational( status.current_aperture, "%.1f"));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "lens max aperture", format_rational( status.lens_max_aperture, "%.1f"));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "lens min aperture", format_rational( status.lens_min_aperture, "%.1f"));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d/%d\n", "set shutter speed", status.set_shutter_speed.nom, status.set_shutter_speed.denom);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "set aperture", format_rational( status.set_aperture, "%.1f"));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "fixed iso", status.fixed_iso);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d-%d\n", "auto iso", status.auto_iso_min,status.auto_iso_max);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "jpeg quality", status.jpeg_quality);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %dM\n", "jpeg resolution", pslr_get_jpeg_resolution( h, status.jpeg_resolution));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "jpeg image tone", get_pslr_jpeg_image_tone_str(status.jpeg_image_tone));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "jpeg saturation", status.jpeg_saturation);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "jpeg contrast", status.jpeg_contrast);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "jpeg sharpness", status.jpeg_sharpness);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "jpeg hue", status.jpeg_hue);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s mm\n", "zoom", format_rational(status.zoom, "%.2f"));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "focus", status.focus);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "color space", get_pslr_color_space_str(status.color_space));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "image format", status.image_format);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "raw format", status.raw_format);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "light meter flags", status.light_meter_flags);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "ec", format_rational( status.ec, "%.2f" ) );
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "custom ev steps", get_pslr_custom_ev_steps_str(status.custom_ev_steps));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "custom sensitivity steps", status.custom_sensitivity_steps);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d (%d)\n", "exposure mode", status.exposure_mode, status.exposure_submode);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "user mode flag", status.user_mode_flag);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "ae metering mode", get_pslr_ae_metering_str(status.ae_metering_mode));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "af mode", get_pslr_af_mode_str(status.af_mode));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "af point select", get_pslr_af_point_sel_str(status.af_point_select));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "selected af point", status.selected_af_point);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "focused af point", status.focused_af_point);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "drive mode", get_pslr_drive_mode_str(status.drive_mode));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "auto bracket mode", status.auto_bracket_mode > 0 ? "on" : "off");
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "auto bracket picture count", status.auto_bracket_picture_count);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "auto bracket ev", format_rational(status.auto_bracket_ev, "%.2f"));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "shake reduction", status.shake_reduction > 0 ? "on" : "off");
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "white balance mode", get_pslr_white_balance_mode_str(status.white_balance_mode));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "white balance adjust", get_white_balance_adjust_str(status.white_balance_adjust_mg, status.white_balance_adjust_ba));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "flash mode", get_pslr_flash_mode_str(status.flash_mode));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %.2f\n", "flash exposure compensation", (1.0 * status.flash_exposure_compensation/256));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %.2f\n", "manual mode ev", (1.0 * status.manual_mode_ev / 10));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "lens", get_lens_name(status.lens_id1, status.lens_id2));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %.2fV %.2fV %.2fV %.2fV\n", "battery", 0.01 * status.battery_1, 0.01 * status.battery_2, 0.01 * status.battery_3, 0.01 * status.battery_4);
    return strbuffer;
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

int ipslr_handle_command_x18( ipslr_handle_t *p, bool cmd9_wrap, int subcommand, int argnum,  ...) {
    if( cmd9_wrap ) {
        CHECK(ipslr_cmd_00_09(p, 1));
    }
    // max 4 args
    va_list ap;
    int args[4];
    int i;
    for( i = 0; i < 4; ++i ) {
	args[i] = 0;
    }
    va_start(ap, argnum);
    for (i = 0; i < argnum; i++) {
	args[i] = va_arg(ap, int);
    }
    va_end(ap);
    CHECK(ipslr_write_args(p, argnum, args[0], args[1], args[2], args[3]));
    CHECK(command(p->fd, 0x18, subcommand, 4 * argnum));
    CHECK(get_status(p->fd));
    if( cmd9_wrap ) {
        CHECK(ipslr_cmd_00_09(p, 2));
    }
    return PSLR_OK;
}

int pslr_test( pslr_handle_t h, bool cmd9_wrap, int subcommand, int argnum,  int arg1, int arg2, int arg3, int arg4) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, cmd9_wrap, subcommand, argnum, arg1, arg2, arg3, arg4);
}

int pslr_set_shutter(pslr_handle_t h, pslr_rational_t value) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_SHUTTER, 2, value.nom, value.denom, 0);
}

int pslr_set_aperture(pslr_handle_t h, pslr_rational_t value) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, false, X18_APERTURE, 3, value.nom, value.denom, 0);
}

int pslr_set_iso(pslr_handle_t h, uint32_t value, uint32_t auto_min_value, uint32_t auto_max_value) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_ISO, 3, value, auto_min_value, auto_max_value);
}

int pslr_set_ec(pslr_handle_t h, pslr_rational_t value) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_EC, 2, value.nom, value.denom, 0);
}

int pslr_set_white_balance(pslr_handle_t h, pslr_white_balance_mode_t wb_mode) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_WHITE_BALANCE, 1, wb_mode, 0, 0);
}

int pslr_set_flash_mode(pslr_handle_t h, pslr_flash_mode_t value) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_FLASH_MODE, 1, value, 0, 0);
}

int pslr_set_flash_exposure_compensation(pslr_handle_t h, pslr_rational_t value) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_FLASH_EXPOSURE_COMPENSATION, 2, value.nom, value.denom, 0);    
}

int pslr_set_drive_mode(pslr_handle_t h, pslr_drive_mode_t drive_mode) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_DRIVE_MODE, 1, drive_mode, 0, 0);    
}

int pslr_set_ae_metering_mode(pslr_handle_t h, pslr_ae_metering_t ae_metering_mode) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_AE_METERING_MODE, 1, ae_metering_mode, 0, 0);    
}

int pslr_set_af_mode(pslr_handle_t h, pslr_af_mode_t af_mode) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_AF_MODE, 1, af_mode, 0, 0);    
}

int pslr_set_af_point_sel(pslr_handle_t h, pslr_af_point_sel_t af_point_sel) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_AF_POINT_SEL, 1, af_point_sel, 0, 0);    
}

int get_hw_jpeg_quality( pslr_handle_t h, int jpeg_stars) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->jpeg_stars - jpeg_stars;
}

int _get_user_jpeg_stars( ipslr_model_info_t *model, int hwqual ) {
    return model->jpeg_stars - hwqual;
}

int pslr_set_jpeg_stars(pslr_handle_t h, int jpeg_stars ) {
    int hwqual;
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if( jpeg_stars > p->model->jpeg_stars ) {
        return PSLR_PARAM;
    }
    hwqual = get_hw_jpeg_quality( h, jpeg_stars );
    return ipslr_handle_command_x18( p, true, X18_JPEG_STARS, 2, 1, hwqual, 0);
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
    return ipslr_handle_command_x18( p, true, X18_JPEG_RESOLUTION, 2, 1, hwres, 0);
}

int pslr_set_jpeg_image_tone(pslr_handle_t h, pslr_jpeg_image_tone_t image_tone) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (image_tone < 0 || image_tone > PSLR_JPEG_IMAGE_TONE_MAX) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, true, X18_JPEG_IMAGE_TONE, 1, image_tone, 0, 0);
}

int pslr_set_jpeg_sharpness(pslr_handle_t h, int32_t sharpness) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int hw_sharpness = sharpness + (pslr_get_model_jpeg_property_levels( h )-1) / 2;
    if (hw_sharpness < 0 || hw_sharpness >=  p->model->jpeg_property_levels) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, false, X18_JPEG_SHARPNESS, 2, 0, hw_sharpness, 0);
}

int pslr_set_jpeg_contrast(pslr_handle_t h, int32_t contrast) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int hw_contrast = contrast + (pslr_get_model_jpeg_property_levels( h )-1) / 2;
    if (hw_contrast < 0 || hw_contrast >=  p->model->jpeg_property_levels) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, false, X18_JPEG_CONTRAST, 2, 0, hw_contrast, 0);
}

int pslr_set_jpeg_hue(pslr_handle_t h, int32_t hue) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int hw_hue = hue + (pslr_get_model_jpeg_property_levels( h )-1) / 2;
    if (hw_hue < 0 || hw_hue >= p->model->jpeg_property_levels) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, false, X18_JPEG_HUE, 2, 0, hw_hue, 0);
}

int pslr_set_jpeg_saturation(pslr_handle_t h, int32_t saturation) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int hw_saturation = saturation + (pslr_get_model_jpeg_property_levels( h )-1) / 2;
    if (hw_saturation < 0 || hw_saturation >=  p->model->jpeg_property_levels) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, false, X18_JPEG_SATURATION, 2, 0, hw_saturation, 0);
}

int pslr_set_image_format(pslr_handle_t h, pslr_image_format_t format) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (format < 0 || format > PSLR_IMAGE_FORMAT_MAX) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, true, X18_IMAGE_FORMAT, 2, 1, format, 0);
}

int pslr_set_raw_format(pslr_handle_t h, pslr_raw_format_t format) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (format < 0 || format > PSLR_RAW_FORMAT_MAX) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, true, X18_RAW_FORMAT, 2, 1, format, 0);
}

int pslr_set_color_space(pslr_handle_t h, pslr_color_space_t color_space) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (color_space < 0 || color_space > PSLR_COLOR_SPACE_MAX) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, true, X18_COLOR_SPACE, 1, color_space, 0, 0);
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

int pslr_dust_removal(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    CHECK(command(p->fd, 0x10, 0x11, 0x00));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

int pslr_button_test(pslr_handle_t h, int bno, int arg) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    CHECK(ipslr_write_args(p, 1, arg));
    CHECK(command(p->fd, 0x10, bno, 4));
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
    return ipslr_handle_command_x18( p, true, X18_EXPOSURE_MODE, 2, 1, mode, 0);
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
    if( p->model->parser_function && (bufs & (1 << bufno)) == 0) {
	// do not check this for limited support cameras
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

//    DPRINT("File offset %d segment: %d offset %d address 0x%x read size %d\n", p->offset, 
//           i, seg_offs, addr, blksz);

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
    DPRINT("buffer get size:%d\n",len);
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
    return ipslr_handle_command_x18( p, true, X18_AF_POINT, 1, point, 0, 0);
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

bool pslr_get_model_only_limited(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->buffer_size == 0 && !p->model->parser_function;
}

int pslr_get_model_fastest_shutter_speed(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->fastest_shutter_speed;
}

int pslr_get_model_base_iso_min(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->base_iso_min;
}

int pslr_get_model_base_iso_max(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->base_iso_max;
}

int pslr_get_model_extended_iso_min(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->extended_iso_min;
}

int pslr_get_model_extended_iso_max(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->extended_iso_max;
}

pslr_jpeg_image_tone_t pslr_get_model_max_supported_image_tone(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->max_supported_image_tone;
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

pslr_buffer_type pslr_get_jpeg_buffer_type(pslr_handle_t h, int jpeg_stars) {
    return 2 + get_hw_jpeg_quality( h, jpeg_stars );
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

int ipslr_status_parse_k10d(ipslr_handle_t *p, pslr_status *status) {
        /* K10D status block */
    uint8_t *buf = p->status_buffer;
        memset(status, 0, sizeof (*status));
        status->bufmask = get_uint16(&buf[0x16]);
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
        status->jpeg_quality = _get_user_jpeg_stars( p->model, get_uint32(&buf[0x80]));
        status->jpeg_image_tone = get_uint32(&buf[0x88]);
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

int ipslr_status_parse_k20d(ipslr_handle_t *p, pslr_status *status) {

    uint8_t *buf = p->status_buffer;
    if( debug ) {
        ipslr_status_diff(buf);
    }
        memset(status, 0, sizeof (*status));
        status->bufmask = get_uint16( &buf[0x16]);
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
        status->jpeg_quality = _get_user_jpeg_stars( p->model, get_uint32(&buf[0x80])); //d
        status->jpeg_image_tone = get_uint32(&buf[0x88]); //d
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

int ipslr_status_parse_istds(ipslr_handle_t *p, pslr_status *status) {

    uint8_t *buf = p->status_buffer;
        /* *ist DS status block */
        memset(status, 0, sizeof (*status));
        status->bufmask = get_uint16(&buf[0x12]);
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

// some of the cameras share most of the status fields
// this method is used for K-x, K-7, K-5, K-r
//
// some cameras also have this data block, but it's shifted a bit
void ipslr_status_parse_common(ipslr_handle_t *p, pslr_status *status, int shift) {

    uint8_t *buf = p->status_buffer;
    status->bufmask = get_uint16( &buf[0x1E + shift]);
    status->user_mode_flag = get_uint32(&buf[0x24 + shift]);
    status->flash_mode = get_uint32(&buf[0x28 + shift]);
    status->flash_exposure_compensation = get_int32(&buf[0x2C + shift]);
    status->set_shutter_speed.nom = get_uint32(&buf[0x34 + shift]);
    status->set_shutter_speed.denom = get_uint32(&buf[0x38 + shift]);
    status->set_aperture.nom = get_uint32(&buf[0x3C + shift]);
    status->set_aperture.denom = get_uint32(&buf[0x40 + shift]);
    status->ec.nom = get_uint32(&buf[0x44 + shift]);
    status->ec.denom = get_uint32(&buf[0x48 + shift]);
    status->auto_bracket_mode = get_uint32(&buf[0x4C + shift]);
    status->auto_bracket_ev.nom = get_uint32(&buf[0x50 + shift]);
    status->auto_bracket_ev.denom = get_uint32(&buf[0x54 + shift]);
    status->auto_bracket_picture_count = get_uint32(&buf[0x58 + shift]);
    status->drive_mode = get_uint32(&buf[0x5C + shift]);
    status->fixed_iso = get_uint32(&buf[0x68 + shift]);
    status->auto_iso_min = get_uint32(&buf[0x6C + shift]);
    status->auto_iso_max = get_uint32(&buf[0x70 + shift]);
    status->white_balance_mode = get_uint32(&buf[0x74 + shift]);
    status->white_balance_adjust_mg = get_uint32(&buf[0x78 + shift]); // 0: M7 7: 0 14: G7
    status->white_balance_adjust_ba = get_uint32(&buf[0x7C + shift]); // 0: B7 7: 0 14: A7
    status->image_format = get_uint32(&buf[0x80 + shift]);
    status->jpeg_resolution = get_uint32(&buf[0x84 + shift]);
    status->jpeg_quality = _get_user_jpeg_stars( p->model, get_uint32(&buf[0x88 + shift]));
    status->raw_format = get_uint32(&buf[0x8C + shift]);
    status->jpeg_image_tone = get_uint32(&buf[0x90 + shift]);
    status->jpeg_saturation = get_uint32(&buf[0x94 + shift]);
    status->jpeg_sharpness = get_uint32(&buf[0x98 + shift]);
    status->jpeg_contrast = get_uint32(&buf[0x9C + shift]);
    status->color_space = get_uint32(&buf[0xA0 + shift]);
    status->custom_ev_steps = get_uint32(&buf[0xA4 + shift]);
    status->custom_sensitivity_steps = get_uint32(&buf[0xa8 + shift]);
    status->exposure_mode = get_uint32(&buf[0xb4 + shift]);
    status->exposure_submode = get_uint32(&buf[0xb8 + shift]);
    status->ae_metering_mode = get_uint32(&buf[0xbc + shift]); // same as cc
    status->af_mode = get_uint32(&buf[0xC0 + shift]);
    status->af_point_select = get_uint32(&buf[0xc4 + shift]);
    status->selected_af_point = get_uint32(&buf[0xc8 + shift]);
    status->shake_reduction = get_uint32(&buf[0xE0 + shift]);
    status->jpeg_hue = get_uint32(&buf[0xFC + shift]);
    status->current_shutter_speed.nom = get_uint32(&buf[0x10C + shift]);
    status->current_shutter_speed.denom = get_uint32(&buf[0x110 + shift]);
    status->current_aperture.nom = get_uint32(&buf[0x114 + shift]);
    status->current_aperture.denom = get_uint32(&buf[0x118 + shift]);
    status->max_shutter_speed.nom = get_uint32(&buf[0x12C + shift]);
    status->max_shutter_speed.denom = get_uint32(&buf[0x130 + shift]);
    status->current_iso = get_uint32(&buf[0x134 + shift]);
    status->light_meter_flags = get_uint32(&buf[0x140 + shift]);
    status->lens_min_aperture.nom = get_uint32(&buf[0x144 + shift]);
    status->lens_min_aperture.denom = get_uint32(&buf[0x148 + shift]);
    status->lens_max_aperture.nom = get_uint32(&buf[0x14C + shift]);
    status->lens_max_aperture.denom = get_uint32(&buf[0x150 + shift]);
    status->manual_mode_ev = get_int32(&buf[0x15C + shift]);
    status->focused_af_point = get_uint32(&buf[0x168 + shift]); //d, unsure about it, a lot is changing when the camera focuses
    // probably voltage*100
    // battery_1 > battery2 ( noload vs load voltage?)   
    status->battery_1 = get_uint32( &buf[0x170 + shift] ); 	 
    status->battery_2 = get_uint32( &buf[0x174 + shift] ); 	 
    status->battery_3 = get_uint32( &buf[0x180 + shift] ); 	 
    status->battery_4 = get_uint32( &buf[0x184 + shift] ); 	 

}

int ipslr_status_parse_kx(ipslr_handle_t *p, pslr_status *status) {

    uint8_t *buf = p->status_buffer;
        /* K-x status block */
    if( debug ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));	
    ipslr_status_parse_common( p, status, 0);
    status->zoom.nom = get_uint32(&buf[0x198]);
    status->zoom.denom = get_uint32(&buf[0x19C]);
    status->focus = get_int32(&buf[0x1A0]);
    status->lens_id1 = (get_uint32( &buf[0x188])) & 0x0F;
    status->lens_id2 = get_uint32( &buf[0x194]);
    return PSLR_OK;
}

// Vince: K-r support 2011-06-22
//
int ipslr_status_parse_kr(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
        /* K-r status block */
    if( debug ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));
    ipslr_status_parse_common( p, status, 0 );
    status->zoom.nom = get_uint32(&buf[0x19C]);
    status->zoom.denom = get_uint32(&buf[0x1A0]);
    status->focus = get_int32(&buf[0x1A4]);
    status->lens_id1 = (get_uint32( &buf[0x18C])) & 0x0F;
    status->lens_id2 = get_uint32( &buf[0x198]);
    return PSLR_OK;
}

int ipslr_status_parse_k5(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
        /* K-x status block */
    if( debug ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));	
    ipslr_status_parse_common( p, status, 0 );
    status->zoom.nom = get_uint32(&buf[0x1A0]);
    status->zoom.denom = get_uint32(&buf[0x1AC]); // or 0x1A4?
    status->focus = get_int32(&buf[0x1A8]); // ?
    status->lens_id1 = (get_uint32( &buf[0x190])) & 0x0F;
    status->lens_id2 = get_uint32( &buf[0x19C]);
    
// TODO: check these fields
//status.exposureLock = getInt32(statusBuf, 0x13C);
//status.focused = getInt32(statusBuf, 0x164);
    return PSLR_OK;   
}

int ipslr_status_parse_km(ipslr_handle_t *p, pslr_status *status) {

    uint8_t *buf = p->status_buffer;
    if( debug ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));	
    ipslr_status_parse_common( p, status, -4);
    status->zoom.nom = get_uint32(&buf[0x180]);
    status->zoom.denom = get_uint32(&buf[0x184]);
    status->lens_id1 = (get_uint32( &buf[0x170])) & 0x0F;
    status->lens_id2 = get_uint32( &buf[0x17c]);
// TODO
// status.exposureLock = getInt32(statusBuf, 0x138);
// status.focused = getInt32(statusBuf, 0x164);
    return PSLR_OK;
}

int ipslr_status_parse_k200d(ipslr_handle_t *p, pslr_status *status) {
        
        
    uint8_t *buf = p->status_buffer;
    if( debug ) {
        ipslr_status_diff(buf);
    }

        memset(status, 0, sizeof (*status));	
        status->bufmask = get_uint16(&buf[0x16]);
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
        status->jpeg_quality = _get_user_jpeg_stars( p->model, get_uint32(&buf[0x80]));
        status->jpeg_image_tone = get_uint32(&buf[0x88]);
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
    DPRINT("read %d bytes\n", n);
    int expected_bufsize = p->model->buffer_size;
    DPRINT("expected_bufsize: %d\n",expected_bufsize);

    CHECK(read_result(p->fd, p->status_buffer, n > MAX_STATUS_BUF_SIZE ? MAX_STATUS_BUF_SIZE: n));

    if( expected_bufsize == 0 || !p->model->parser_function ) {
        // limited support only
        return PSLR_OK;
    } else if( expected_bufsize > 0 && expected_bufsize != n ) {
        DPRINT("Waiting for %d bytes but got %d\n", expected_bufsize, n);
        return PSLR_READ_ERROR;
    } else {
        // everything OK
        int ret = (*p->model->parser_function)(p, status);
        // required for K-x, probably for other cameras too
        status->exposure_mode = exposure_mode_conversion( status->exposure_mode );
        return ret;
    }
}

// fullpress: take picture
// halfpress: autofocus
static int ipslr_press_shutter(ipslr_handle_t *p, bool fullpress) {
    int r;
    CHECK(ipslr_status_full(p, &p->status));
    DPRINT("before: mask=0x%x\n", p->status.bufmask);
    CHECK(ipslr_write_args(p, 1, fullpress ? 2 : 1));
    CHECK(command(p->fd, 0x10, 0x05, 0x04));
    r = get_status(p->fd);
    DPRINT("shutter result code: 0x%x\n", r);
    return PSLR_OK;
}

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

static uint16_t get_uint16(uint8_t *buf) {
    uint16_t res;
    res = buf[0] << 8 | buf[1];
    return res;
}

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


/* -----------------------------------------------------------------------
 write_debug
----------------------------------------------------------------------- */
void write_debug( const char* message, ... ){

    // Be sure debug is really on as DPRINT doesn't know
    //
    if( !debug ) return;

    // Write to stderr
    //
    va_list argp;
    va_start(argp, message);
    vfprintf( stderr, message, argp );
    va_end(argp);
    return;
}
