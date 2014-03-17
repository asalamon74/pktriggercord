/*
    pkTriggerCord
    Copyright (C) 2011-2014 Andras Salamon <andras.salamon@melda.info>
    Remote control of Pentax DSLR cameras.

    Support for K200D added by Jens Dreyer <jens.dreyer@udo.edu> 04/2011
    Support for K-r added by Vincenc Podobnik <vincenc.podobnik@gmail.com> 06/2011
    Support for K-30 added by Camilo Polymeris <cpolymeris@gmail.com> 09/2012
    Support for K-01 added by Ethan Queen <ethanqueen@gmail.com> 01/2013


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
#include <math.h>

#include "pslr.h"
#include "pslr_scsi.h"
#include "pslr_lens.h"

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

void sleep_sec(double sec) {
    int i;
    for(i=0; i<floor(sec); ++i) {
	usleep(999999); // 1000000 is not working for Windows
    }
    usleep(1000000*(sec-floor(sec)));
}

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

static pslr_progress_callback_t progress_callback = NULL;

user_file_format_t file_formats[3] = {
    { USER_FILE_FORMAT_PEF, "PEF", "pef"},
    { USER_FILE_FORMAT_DNG, "DNG", "dng"},
    { USER_FILE_FORMAT_JPEG, "JPEG", "jpg"},
};


const char* valid_vendors[3] = {"PENTAX", "SAMSUNG", "RICOHIMG"};
const char* valid_models[2] = {"DIGITAL_CAMERA", "DSC"}; // no longer list all of them, DSC* should be ok

// x18 subcommands to change camera properties
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
    X18_WHITE_BALANCE_ADJ,
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

// x10 subcommands for buttons
// X10_n: unknown effect
typedef enum {
    X10_00,
    X10_01,
    X10_02,
    X10_03,
    X10_04,
    X10_SHUTTER,
    X10_AE_LOCK,
    X10_GREEN,
    X10_AE_UNLOCK,
    X10_09,
    X10_CONNECT,
    X10_0B,
    X10_CONTINUOUS,
    X10_BULB,
    X10_0E,
    X10_0F,
    X10_10,
    X10_DUST
} x10_subcommands_t;

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

// most of the cameras require this exposure mode conversion step
pslr_gui_exposure_mode_t exposure_mode_conversion( pslr_exposure_mode_t exp ) {
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
    case PSLR_EXPOSURE_MODE_AV_OFFAUTO:
	return PSLR_GUI_EXPOSURE_MODE_AV;
    case PSLR_EXPOSURE_MODE_TAV:
	return PSLR_GUI_EXPOSURE_MODE_TAV;
    case PSLR_EXPOSURE_MODE_M:
    case PSLR_EXPOSURE_MODE_M_OFFAUTO:
	return PSLR_GUI_EXPOSURE_MODE_M;
    case PSLR_EXPOSURE_MODE_B:
    case PSLR_EXPOSURE_MODE_B_OFFAUTO:
	return PSLR_GUI_EXPOSURE_MODE_B;
    case PSLR_EXPOSURE_MODE_X:
	return PSLR_GUI_EXPOSURE_MODE_X;
    case PSLR_EXPOSURE_MODE_MAX:
	return PSLR_GUI_EXPOSURE_MODE_MAX;
    }    
    return 0;
}

pslr_handle_t pslr_init( char *model, char *device ) {
    int fd;
    char vendorId[20];
    char productId[20];
    int driveNum;
    char **drives;
    const char *camera_name;

    if( device == NULL ) {
	drives = get_drives(&driveNum);
    } else {
	driveNum = 1;
	drives = malloc( driveNum * sizeof(char*) );
        drives[0] = malloc( sizeof( device ) );
	strncpy( drives[0], device, sizeof( device ) );
    }
    int i;
    for( i=0; i<driveNum; ++i ) {
	pslr_result result = get_drive_info( drives[i], &fd, vendorId, sizeof(vendorId), productId, sizeof(productId));

	DPRINT("Checking drive:  %s %s %s\n", drives[i], vendorId, productId);
	if( find_in_array( valid_vendors, sizeof(valid_vendors)/sizeof(valid_vendors[0]),vendorId) != -1 
	    && find_in_array( valid_models, sizeof(valid_models)/sizeof(valid_models[0]), productId) != -1 ) {
	    if( result == PSLR_OK ) {
		DPRINT("Found camera %s %s\n", vendorId, productId);
		pslr.fd = fd;
		if( model != NULL ) {
		    // user specified the camera model
		    camera_name = pslr_camera_name( &pslr );
		    DPRINT("Name of the camera: %s\n", camera_name);
		    if( str_comparison_i( camera_name, model, strlen( camera_name) ) == 0 ) {
			return &pslr;	    
		    } else {
			DPRINT("Ignoring camera %s %s\n", vendorId, productId);
			pslr_shutdown ( &pslr );
			pslr.id = 0;
			pslr.model = NULL;
		    }
		} else {
		    return &pslr;	    
		}
	    } else {
		DPRINT("Cannot get drive info of Pentax camera. Please do not forget to install the program using 'make install'\n");
		// found the camera but communication is not possible
		close( fd );
		continue;
	    }
	} else {
	    close_drive( &fd );
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
    if( !p->model ) {
      DPRINT("Unknown Pentax camera.\n");
      return 1;
    }      
    CHECK(ipslr_status_full(p, &p->status));
    DPRINT("init bufmask=0x%x\n", p->status.bufmask);
    if( !p->model->old_scsi_command ) {
        CHECK(ipslr_cmd_00_09(p, 2));
    }
    DPRINT("before status1 full\n");
    CHECK(ipslr_status_full(p, &p->status));
    DPRINT("after status1 full\n");
    CHECK(ipslr_cmd_10_0a(p, 1));
    if( p->model->old_scsi_command ) {
        CHECK(ipslr_cmd_00_05(p));
    }
    DPRINT("before status2 full\n");
    CHECK(ipslr_status_full(p, &p->status));
    DPRINT("after status2 full\n");
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
    close_drive(&p->fd);
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
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "raw format", get_pslr_raw_format_str(status.raw_format));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "light meter flags", status.light_meter_flags);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "ec", format_rational( status.ec, "%.2f" ) );
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "custom ev steps", get_pslr_custom_ev_steps_str(status.custom_ev_steps));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "custom sensitivity steps", status.custom_sensitivity_steps);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d (%s)\n", "exposure mode", status.exposure_mode, get_pslr_exposure_submode_str(status.exposure_submode));
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
    return ipslr_handle_command_x18( p, true, X18_WHITE_BALANCE, 1, wb_mode);
}

int pslr_set_white_balance_adjustment(pslr_handle_t h, pslr_white_balance_mode_t wb_mode, uint32_t wbadj_mg, uint32_t wbadj_ba) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_WHITE_BALANCE_ADJ, 3, wb_mode, wbadj_mg, wbadj_ba);
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

int pslr_set_jpeg_stars(pslr_handle_t h, int jpeg_stars ) {
    int hwqual;
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if( jpeg_stars > p->model->max_jpeg_stars ) {
        return PSLR_PARAM;
    }
    hwqual = get_hw_jpeg_quality( p->model, jpeg_stars );
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
    CHECK(command(p->fd, 0x10, X10_GREEN, 0x00));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

int pslr_dust_removal(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    CHECK(command(p->fd, 0x10, X10_DUST, 0x00));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

int pslr_bulb(pslr_handle_t h, bool on ) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    CHECK(ipslr_write_args(p, 1, on ? 1 : 0));
    CHECK(command(p->fd, 0x10, X10_BULB, 0x04));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

int pslr_button_test(pslr_handle_t h, int bno, int arg) {
    int r;
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    CHECK(ipslr_write_args(p, 1, arg));
    CHECK(command(p->fd, 0x10, bno, 4));
    r = get_status(p->fd);
    DPRINT("button result code: 0x%x\n", r);
    return PSLR_OK;
}


int pslr_ae_lock(pslr_handle_t h, bool lock) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (lock)
        CHECK(command(p->fd, 0x10, X10_AE_LOCK, 0x00));
    else
        CHECK(command(p->fd, 0x10, X10_AE_UNLOCK, 0x00));
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

int pslr_get_model_max_jpeg_stars(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->max_jpeg_stars;
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

bool pslr_get_model_need_exposure_conversion(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->need_exposure_mode_conversion;
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
    if (p->id == 0) {
        ret = ipslr_identify(p);
        if (ret != PSLR_OK)
            return NULL;
    }
    if (p->model)
        return p->model->name;
    else {
        static char unk_name[256];
        snprintf(unk_name, sizeof (unk_name), "ID#%x", p->id);
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
    CHECK(command(p->fd, 0x10, X10_CONNECT, 4));
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

static int ipslr_status_full(ipslr_handle_t *p, pslr_status *status) {
    int n;
    CHECK(command(p->fd, 0, 8, 0));
    n = get_result(p->fd);
    DPRINT("read %d bytes\n", n);
    int expected_bufsize = p->model != NULL ? p->model->buffer_size : 0;
    if( p->model == NULL ) {
      DPRINT("p model null\n");     
    }
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
        (*p->model->parser_function)(p, status);
	if( p->model->need_exposure_mode_conversion ) {
            status->exposure_mode = exposure_mode_conversion( status->exposure_mode );
	}
        return PSLR_OK;
    }
}

// fullpress: take picture
// halfpress: autofocus
static int ipslr_press_shutter(ipslr_handle_t *p, bool fullpress) {
    int r;
    CHECK(ipslr_status_full(p, &p->status));
    DPRINT("before: mask=0x%x\n", p->status.bufmask);
    CHECK(ipslr_write_args(p, 1, fullpress ? 2 : 1));
    CHECK(command(p->fd, 0x10, X10_SHUTTER, 0x04));
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
    int num_try = 20;

    pInfo->b = 0;
    while( pInfo->b == 0 && --num_try > 0 ) {
        CHECK(command(p->fd, 0x04, 0x00, 0x00));
        n = get_result(p->fd);
        if (n != 16) {
            return PSLR_READ_ERROR;
        }
        CHECK(read_result(p->fd, buf, 16));
        pInfo->a = get_uint32(&buf[0]);
        pInfo->b = get_uint32(&buf[4]);
        pInfo->addr = get_uint32(&buf[8]);
        pInfo->length = get_uint32(&buf[12]);
	if( pInfo-> b == 0 ) {
	  DPRINT("Waiting for segment info addr: 0x%x len: %d B=%d\n", pInfo->addr, pInfo->length, pInfo->b);
	  sleep_sec( 0.1 );
	}
    }
    return PSLR_OK;
}

static int ipslr_download(ipslr_handle_t *p, uint32_t addr, uint32_t length, uint8_t *buf) {
    uint8_t downloadCmd[8] = {0xf0, 0x24, 0x06, 0x02, 0x00, 0x00, 0x00, 0x00};
    uint32_t block;
    int n;
    int retry;
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
        get_status(p->fd);

        n = scsi_read(p->fd, downloadCmd, sizeof (downloadCmd), buf, block);
        get_status(p->fd);

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

    CHECK(command(p->fd, 0, 4, 0));
    n = get_result(p->fd);
    if (n != 8)
        return PSLR_READ_ERROR;
    CHECK(read_result(p->fd, idbuf, 8));
    p->id = get_uint32(&idbuf[0]);
    DPRINT("id of the camera: %x\n", p->id);
    p->model = find_model_by_id( p->id );
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
        DPRINT("get_status->\n");
        hexdump(statusbuf, 8);
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
    return statusbuf[0] | statusbuf[1] << 8 | statusbuf[2] << 16 | statusbuf[3] << 24;
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
