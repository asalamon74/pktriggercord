/*
    pkTriggerCord
    Copyright (C) 2011-2013 Andras Salamon <andras.salamon@melda.info>
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
#ifndef PSLR_MODEL_H
#define PSLR_MODEL_H

#include "pslr_enum.h"
#include "pslr_scsi.h"

#define MAX_RESOLUTION_SIZE 4
#define MAX_STATUS_BUF_SIZE 452
#define MAX_SEGMENTS 4

typedef struct ipslr_handle ipslr_handle_t;

typedef struct {
    int32_t nom;
    int32_t denom;
} pslr_rational_t;

typedef struct {
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
    uint32_t jpeg_image_tone;
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
    uint32_t exposure_submode;
    uint32_t user_mode_flag;
    uint32_t ae_metering_mode;
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
    uint32_t battery_1;
    uint32_t battery_2;
    uint32_t battery_3;
    uint32_t battery_4;
} pslr_status;

typedef void (*ipslr_status_parse_t)(ipslr_handle_t *p, pslr_status *status);

typedef struct {
    uint32_t id1;                                    // Pentax model ID
    const char *name;                                // name
    bool old_scsi_command;                           // 1 for *ist cameras, 0 for the newer cameras
    bool need_exposure_mode_conversion;              // is exposure_mode_conversion required
    int buffer_size;                                 // buffer size in bytes
    int max_jpeg_stars;                              // maximum jpeg stars
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

typedef struct {
    uint32_t offset;
    uint32_t addr;
    uint32_t length;
} ipslr_segment_t;

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

void ipslr_status_parse_kx   (ipslr_handle_t *p, pslr_status *status);
void ipslr_status_parse_kr   (ipslr_handle_t *p, pslr_status *status);
void ipslr_status_parse_k20d (ipslr_handle_t *p, pslr_status *status);
void ipslr_status_parse_k10d (ipslr_handle_t *p, pslr_status *status);
void ipslr_status_parse_k200d(ipslr_handle_t *p, pslr_status *status);
void ipslr_status_parse_istds(ipslr_handle_t *p, pslr_status *status);
void ipslr_status_parse_k5   (ipslr_handle_t *p, pslr_status *status);
void ipslr_status_parse_k30  (ipslr_handle_t *p, pslr_status *status);
void ipslr_status_parse_km   (ipslr_handle_t *p, pslr_status *status);
void ipslr_status_parse_k01  (ipslr_handle_t *p, pslr_status *status);

ipslr_model_info_t *find_model_by_id( uint32_t id );

int get_hw_jpeg_quality( ipslr_model_info_t *model, int user_jpeg_stars);

uint16_t get_uint16(uint8_t *buf);
uint32_t get_uint32(uint8_t *buf);
int32_t get_int32(uint8_t *buf);

void hexdump(uint8_t *buf, uint32_t bufLen);

#endif
