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
#include <stdbool.h>

#include "pslr_model.h"

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

uint16_t get_uint16(uint8_t *buf) {
    uint16_t res;
    res = buf[0] << 8 | buf[1];
    return res;
}

uint32_t get_uint32(uint8_t *buf) {
    uint32_t res;
    res = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
    return res;
}

int32_t get_int32(uint8_t *buf) {
    int32_t res;
    res = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
    return res;
}

void hexdump(uint8_t *buf, uint32_t bufLen) {
    uint32_t i;
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

int _get_user_jpeg_stars( ipslr_model_info_t *model, int hwqual ) {
    if( model->id == 0x12f71 ) {
        // K5IIs hack
        // TODO: test it
        if( hwqual == model->max_jpeg_stars -1 ) {
            return model->max_jpeg_stars;
        } else {
            return model->max_jpeg_stars - 1 - hwqual;
	}
    } else {
        return model->max_jpeg_stars - hwqual;
    }
}

int get_hw_jpeg_quality( ipslr_model_info_t *model, int user_jpeg_stars) {
    if( model->id == 0x12f71 ) {
        // K5IIs hack
        // TODO: test it
        if( user_jpeg_stars == model->max_jpeg_stars ) {
            return model->max_jpeg_stars-1;
        } else {
            return model->max_jpeg_stars - 1 - user_jpeg_stars;
	}
    } else {
        return model->max_jpeg_stars - user_jpeg_stars;
    }
}


void ipslr_status_parse_k10d(ipslr_handle_t  *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
    if( debug ) {
        ipslr_status_diff(buf);
    }
    memset(status, 0, sizeof (*status));
    status->bufmask = get_uint16(&buf[0x16]);
    status->user_mode_flag = get_uint32(&buf[0x1c]);
    status->set_shutter_speed.nom = get_uint32(&buf[0x2c]);
    status->set_shutter_speed.denom = get_uint32(&buf[0x30]);
    status->set_aperture.nom = get_uint32(&buf[0x34]);
    status->set_aperture.denom = get_uint32(&buf[0x38]);
    status->ec.nom = get_uint32(&buf[0x3c]);
    status->ec.denom = get_uint32(&buf[0x40]);
    status->fixed_iso = get_uint32(&buf[0x60]);
    status->image_format = get_uint32(&buf[0x78]);
    status->jpeg_resolution = get_uint32(&buf[0x7c]);
    status->jpeg_quality = _get_user_jpeg_stars( p->model, get_uint32(&buf[0x80]));
    status->raw_format = get_uint32(&buf[0x84]);
    status->jpeg_image_tone = get_uint32(&buf[0x88]);
    status->jpeg_saturation = get_uint32(&buf[0x8c]);
    status->jpeg_sharpness = get_uint32(&buf[0x90]);
    status->jpeg_contrast = get_uint32(&buf[0x94]);
    status->custom_ev_steps = get_uint32(&buf[0x9c]);
    status->custom_sensitivity_steps = get_uint32(&buf[0xa0]);
    status->af_point_select = get_uint32(&buf[0xbc]);
    status->selected_af_point = get_uint32(&buf[0xc0]);
    status->exposure_mode = get_uint32(&buf[0xac]);
    status->current_shutter_speed.nom = get_uint32(&buf[0xf4]);
    status->current_shutter_speed.denom = get_uint32(&buf[0xf8]);
    status->current_aperture.nom = get_uint32(&buf[0xfc]);
    status->current_aperture.denom = get_uint32(&buf[0x100]);
    status->current_iso = get_uint32(&buf[0x11c]);
    status->light_meter_flags = get_uint32(&buf[0x124]);
    status->lens_min_aperture.nom = get_uint32(&buf[0x12c]);
    status->lens_min_aperture.denom = get_uint32(&buf[0x130]);
    status->lens_max_aperture.nom = get_uint32(&buf[0x134]);
    status->lens_max_aperture.denom = get_uint32(&buf[0x138]);
    status->focused_af_point = get_uint32(&buf[0x150]);
    status->zoom.nom = get_uint32(&buf[0x16c]);
    status->zoom.denom = get_uint32(&buf[0x170]);
    status->focus = get_int32(&buf[0x174]);
}

void ipslr_status_parse_k20d(ipslr_handle_t *p, pslr_status *status) {

    uint8_t *buf = p->status_buffer;
    if( debug ) {
        ipslr_status_diff(buf);
    }
    memset(status, 0, sizeof (*status));
    status->bufmask = get_uint16( &buf[0x16]);
    status->user_mode_flag = get_uint32(&buf[0x1c]);
    status->set_shutter_speed.nom = get_uint32(&buf[0x2c]);
    status->set_shutter_speed.denom = get_uint32(&buf[0x30]);
    status->set_aperture.nom = get_uint32(&buf[0x34]);
    status->set_aperture.denom = get_uint32(&buf[0x38]);
    status->ec.nom = get_uint32(&buf[0x3c]);
    status->ec.denom = get_uint32(&buf[0x40]);
    status->fixed_iso = get_uint32(&buf[0x60]);
    status->image_format = get_uint32(&buf[0x78]);
    status->jpeg_resolution = get_uint32(&buf[0x7c]);
    status->jpeg_quality = _get_user_jpeg_stars( p->model, get_uint32(&buf[0x80]));
    status->raw_format = get_uint32(&buf[0x84]);
    status->jpeg_image_tone = get_uint32(&buf[0x88]);
    status->jpeg_saturation = get_uint32(&buf[0x8c]); // commands do now work for it?
    status->jpeg_sharpness = get_uint32(&buf[0x90]); // commands do now work for it?
    status->jpeg_contrast = get_uint32(&buf[0x94]); // commands do now work for it?
    status->custom_ev_steps = get_uint32(&buf[0x9c]);
    status->custom_sensitivity_steps = get_uint32(&buf[0xa0]);
    status->ae_metering_mode = get_uint32(&buf[0xb4]); // same as c4
    status->af_mode = get_uint32(&buf[0xb8]);
    status->af_point_select = get_uint32(&buf[0xbc]); // not sure
    status->selected_af_point = get_uint32(&buf[0xc0]);
    status->exposure_mode = get_uint32(&buf[0xac]);
    status->current_shutter_speed.nom = get_uint32(&buf[0x108]);
    status->current_shutter_speed.denom = get_uint32(&buf[0x10C]);
    status->current_aperture.nom = get_uint32(&buf[0x110]);
    status->current_aperture.denom = get_uint32(&buf[0x114]);
    status->current_iso = get_uint32(&buf[0x130]);
    status->light_meter_flags = get_uint32(&buf[0x138]);
    status->lens_min_aperture.nom = get_uint32(&buf[0x140]);
    status->lens_min_aperture.denom = get_uint32(&buf[0x144]);
    status->lens_max_aperture.nom = get_uint32(&buf[0x148]);
    status->lens_max_aperture.denom = get_uint32(&buf[0x14B]);
    status->focused_af_point = get_uint32(&buf[0x160]); // unsure about it, a lot is changing when the camera focuses
    status->zoom.nom = get_uint32(&buf[0x180]);
    status->zoom.denom = get_uint32(&buf[0x184]);
    status->focus = get_int32(&buf[0x188]); // current focus ring position?
    // 0x158 current ev?
    // 0x160 and 0x164 change when AF
}

void ipslr_status_parse_istds(ipslr_handle_t *p, pslr_status *status) {

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
    
    // no DNG support so raw format is PEF
    status->raw_format = PSLR_RAW_FORMAT_PEF;
}

// some of the cameras share most of the status fields
// this method is used for K-x, K-7, K-5, K-r
//
// some cameras also have this data block, but it's shifted a bit
void ipslr_status_parse_common(ipslr_handle_t *p, pslr_status *status, int shift) {

    uint8_t *buf = p->status_buffer;
    // 0x0C: 0x85 0xA5
    // 0x0F: beginning 0 sometime changes to 1
    // 0x14: LCD panel 2: turned off 3: on?
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
    status->light_meter_flags = get_uint32(&buf[0x13C + shift]);
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

void ipslr_status_parse_kx(ipslr_handle_t *p, pslr_status *status) {

    uint8_t *buf = p->status_buffer;
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
}

// Vince: K-r support 2011-06-22
//
void ipslr_status_parse_kr(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
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
}

void ipslr_status_parse_k5(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
    if( debug ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));	
    ipslr_status_parse_common( p, status, 0 );
    status->zoom.nom = get_uint32(&buf[0x1A0]);
    status->zoom.denom = get_uint32(&buf[0x1A4]);
    status->focus = get_int32(&buf[0x1A8]); // ?
    status->lens_id1 = (get_uint32( &buf[0x190])) & 0x0F;
    status->lens_id2 = get_uint32( &buf[0x19C]);
    
// TODO: check these fields
//status.focused = getInt32(statusBuf, 0x164);
}

void ipslr_status_parse_k30(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
    if( debug ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));	
    ipslr_status_parse_common( p, status, 0 );
    //~ status->jpeg_contrast -= 4;
    //~ status->jpeg_hue -= 4;
    //~ status->jpeg_sharpness -= 4;
    //~ status->jpeg_saturation -= 4;
    status->zoom.nom = get_uint32(&buf[0x1A0]);
    status->zoom.denom = 100;
    status->focus = get_int32(&buf[0x1A8]); // ?
    status->lens_id1 = (get_uint32( &buf[0x190])) & 0x0F;
    status->lens_id2 = get_uint32( &buf[0x19C]);
}

// status check seems to be the same as K30
void ipslr_status_parse_k01(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
    if( debug ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));
    ipslr_status_parse_common( p, status, 0 );
    //~ status->jpeg_contrast -= 4;
    //~ status->jpeg_hue -= 4;
    //~ status->jpeg_sharpness -= 4;
    //~ status->jpeg_saturation -= 4;
    status->zoom.nom = get_uint32(&buf[0x1A0]); // - good for K01
    status->zoom.denom = 100; // good for K-01
    status->focus = get_int32(&buf[0x1A8]); // ? - good for K01
    status->lens_id1 = (get_uint32( &buf[0x190])) & 0x0F; // - good for K01
    status->lens_id2 = get_uint32( &buf[0x19C]); // - good for K01
}

void ipslr_status_parse_km(ipslr_handle_t *p, pslr_status *status) {
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
// status.focused = getInt32(statusBuf, 0x164);
}

void ipslr_status_parse_k200d(ipslr_handle_t *p, pslr_status *status) {               
    uint8_t *buf = p->status_buffer;
    if( debug ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));	
    status->bufmask = get_uint16(&buf[0x16]);
    status->user_mode_flag = get_uint32(&buf[0x1c]);
    status->set_shutter_speed.nom = get_uint32(&buf[0x2c]);
    status->set_shutter_speed.denom = get_uint32(&buf[0x30]);
    status->current_aperture.nom = get_uint32(&buf[0x034]); 
    status->current_aperture.denom = get_uint32(&buf[0x038]); 
    status->set_aperture.nom = get_uint32(&buf[0x34]);
    status->set_aperture.denom = get_uint32(&buf[0x38]);
    status->ec.nom = get_uint32(&buf[0x3c]);
    status->ec.denom = get_uint32(&buf[0x40]);
    status->current_iso = get_uint32(&buf[0x060]); 
    status->fixed_iso = get_uint32(&buf[0x60]);
    status->auto_iso_min = get_uint32(&buf[0x64]);
    status->auto_iso_max = get_uint32(&buf[0x68]);
    status->image_format = get_uint32(&buf[0x78]);
    status->jpeg_resolution = get_uint32(&buf[0x7c]);
    status->jpeg_quality = _get_user_jpeg_stars( p->model, get_uint32(&buf[0x80]));
    status->raw_format = get_uint32(&buf[0x84]);
    status->jpeg_image_tone = get_uint32(&buf[0x88]);
    status->jpeg_saturation = get_uint32(&buf[0x8c]);
    status->jpeg_sharpness = get_uint32(&buf[0x90]);
    status->jpeg_contrast = get_uint32(&buf[0x94]);
    //status->custom_ev_steps = get_uint32(&buf[0x9c]);
    //status->custom_sensitivity_steps = get_uint32(&buf[0xa0]);
    status->exposure_mode = get_uint32(&buf[0xac]);
    status->af_mode = get_uint32(&buf[0xb8]);
    status->af_point_select = get_uint32(&buf[0xbc]);
    status->selected_af_point = get_uint32(&buf[0xc0]);
    status->drive_mode = get_uint32(&buf[0xcc]); 
    status->shake_reduction = get_uint32(&buf[0xda]);
    status->jpeg_hue = get_uint32(&buf[0xf4]);
    status->current_shutter_speed.nom = get_uint32(&buf[0x0104]); 
    status->current_shutter_speed.denom = get_uint32(&buf[0x108]); 
    status->light_meter_flags = get_uint32(&buf[0x124]);
    status->lens_min_aperture.nom = get_uint32(&buf[0x13c]);
    status->lens_min_aperture.denom = get_uint32(&buf[0x140]);
    status->lens_max_aperture.nom = get_uint32(&buf[0x144]); 
    status->lens_max_aperture.denom = get_uint32(&buf[0x148]); 
    status->focused_af_point = get_uint32(&buf[0x150]);
    status->zoom.nom = get_uint32(&buf[0x17c]);
    status->zoom.denom = get_uint32(&buf[0x180]);
    status->focus = get_int32(&buf[0x184]);
    // Drive mode: 0=Single shot, 1= Continous Hi, 2= Continous Low or Self timer 12s, 3=Self timer 2s
    // 4= remote, 5= remote 3s delay
}

ipslr_model_info_t camera_models[] = {
    { 0x12aa2, "*ist DS",     1, 1, 264, 3, {6, 4, 2},      5, 4000, 200, 3200, 200,  3200, PSLR_JPEG_IMAGE_TONE_BRIGHT,        ipslr_status_parse_istds },
    { 0x12cd2, "K20D",        0, 1, 412, 4, {14, 10, 6, 2}, 7, 4000, 100, 3200, 100,  6400, PSLR_JPEG_IMAGE_TONE_MONOCHROME,    ipslr_status_parse_k20d  },
    { 0x12c1e, "K10D",        0, 1, 392, 3, {10, 6, 2},     7, 4000, 100, 1600, 100,  1600, PSLR_JPEG_IMAGE_TONE_BRIGHT,        ipslr_status_parse_k10d  },
    { 0x12c20, "GX10",        0, 1, 392, 3, {10, 6, 2},     7, 4000, 100, 1600, 100,  1600, PSLR_JPEG_IMAGE_TONE_BRIGHT,        ipslr_status_parse_k10d  },
    { 0x12cd4, "GX20",        0, 1, 412, 4, {14, 10, 6, 2}, 7, 4000, 100, 3200, 100,  6400, PSLR_JPEG_IMAGE_TONE_MONOCHROME,    ipslr_status_parse_k20d  },
    { 0x12dfe, "K-x",         0, 1, 436, 3, {12, 10, 6, 2}, 9, 6000, 200, 6400, 100, 12800, PSLR_JPEG_IMAGE_TONE_MUTED,         ipslr_status_parse_kx    },
    { 0x12cfa, "K200D",       0, 1, 408, 3, {10, 6, 2},     9, 4000, 100, 1600, 100,  1600, PSLR_JPEG_IMAGE_TONE_MONOCHROME,    ipslr_status_parse_k200d }, 
    { 0x12db8, "K-7",         0, 1, 436, 4, {14, 10, 6, 2}, 9, 8000, 100, 3200, 100,  6400, PSLR_JPEG_IMAGE_TONE_MUTED,         ipslr_status_parse_kx    },
    { 0x12e6c, "K-r",         0, 1, 440, 3, {12, 10, 6, 2}, 9, 6000, 200,12800, 100, 25600, PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS, ipslr_status_parse_kr    },
    { 0x12e76, "K-5",         0, 1, 444, 4, {16, 10, 6, 2}, 9, 8000, 100,12800,  80, 51200, PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS, ipslr_status_parse_k5    },
    { 0x12d72, "K-2000",      0, 1, 412, 3, {10, 6, 2},     9, 4000, 100, 3200, 100,  3200, PSLR_JPEG_IMAGE_TONE_MONOCHROME,    ipslr_status_parse_km    },
    { 0x12d73, "K-m",         0, 1, 412, 3, {10, 6, 2},     9, 4000, 100, 3200, 100,  3200, PSLR_JPEG_IMAGE_TONE_MONOCHROME,    ipslr_status_parse_km    },
    { 0x12f52, "K-30",        0, 0, 452, 3, {16, 12, 8, 5}, 9, 6000, 100,12800, 100, 25600, PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS, ipslr_status_parse_k30   },
    { 0x12ef8, "K-01",        0, 1, 452, 3, {16, 12, 8, 5}, 9, 4000, 100,12800, 100, 25600, PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS, ipslr_status_parse_k01   },
    { 0x12f70, "K-5II",       0, 1, 444,  4, {16, 10, 6, 2}, 9, 8000, 100, 12800, 80, 51200, PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS, ipslr_status_parse_k5   },
    { 0x12f71, "K-5IIs",      0, 1, 444,  4, {16, 10, 6, 2}, 9, 8000, 100, 12800, 80, 51200, PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS, ipslr_status_parse_k5   },
// only limited support from here
    { 0x12994, "*ist D",      1, 1, 0,   3, {6, 4, 2}, 3, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_NONE  , NULL}, // buffersize: 264 
    { 0x12b60, "*ist DS2",    1, 1, 0,   3, {6, 4, 2}, 5, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_BRIGHT, NULL},
    { 0x12b1a, "*ist DL",     1, 1, 0,   3, {6, 4, 2}, 5, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_BRIGHT, NULL},
    { 0x12b80, "GX-1L",       1, 1, 0,   3, {6, 4, 2}, 5, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_BRIGHT, NULL},
    { 0x12b9d, "K110D",       0, 1, 0,   3, {6, 4, 2}, 5, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_BRIGHT, NULL},
    { 0x12b9c, "K100D",       1, 1, 0,   3, {6, 4, 2}, 5, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_BRIGHT, NULL},
    { 0x12ba2, "K100D Super", 1, 1, 0,   3, {6, 4, 2}, 5, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_BRIGHT, NULL},
};

ipslr_model_info_t *find_model_by_id( uint32_t id ) {
    int i;
    for( i = 0; i<sizeof (camera_models) / sizeof (camera_models[0]); i++) {
        if( camera_models[i].id == id ) {
            return &camera_models[i];
        }
    }
    return NULL;
}
