/*
    pkTriggerCord
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2011-2019 Andras Salamon <andras.salamon@melda.info>

    based on:

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

#include "libpktriggercord.h"

bool debug = false;
bool warnings = false;

bool bulb_timer_before=false;
bool astrotracer_before=false;

int save_buffer(pslr_handle_t camhandle, int bufno, int fd, pslr_status *status, user_file_format filefmt, int jpeg_stars) {
    pslr_buffer_type imagetype;
    uint8_t buf[65536];
    uint32_t length;
    uint32_t current;

    if (filefmt == USER_FILE_FORMAT_PEF) {
        imagetype = PSLR_BUF_PEF;
    } else if (filefmt == USER_FILE_FORMAT_DNG) {
        imagetype = PSLR_BUF_DNG;
    } else {
        imagetype = pslr_get_jpeg_buffer_type( camhandle, jpeg_stars );
    }

    DPRINT("get buffer %d type %d res %d\n", bufno, imagetype, status->jpeg_resolution);

    if (pslr_buffer_open(camhandle, bufno, imagetype, status->jpeg_resolution) != PSLR_OK) {
        return 1;
    }

    length = pslr_buffer_get_size(camhandle);
    DPRINT("Buffer length: %d\n", length);
    current = 0;

    while (true) {
        uint32_t bytes;
        bytes = pslr_buffer_read(camhandle, buf, sizeof (buf));
        if (bytes == 0) {
            break;
        }
        ssize_t r = write(fd, buf, bytes);
        if (r == 0) {
            DPRINT("write(buf): Nothing has been written to buf.\n");
        } else if (r == -1) {
            perror("write(buf)");
        } else if (r < bytes) {
            DPRINT("write(buf): only write %zu bytes, should be %d bytes.\n", r, bytes);
        }
        current += bytes;
    }
    pslr_buffer_close(camhandle);
    return 0;
}

void save_memory(pslr_handle_t camhandle, int fd, uint32_t length) {
    uint8_t buf[65536];
    uint32_t current;

    DPRINT("save memory %d\n", length);

    current = 0;

    while (current<length) {
        uint32_t bytes;
        int readsize=length-current>65536 ? 65536 : length-current;
        bytes = pslr_fullmemory_read(camhandle, buf, current, readsize);
        if (bytes == 0) {
            break;
        }
        ssize_t r = write(fd, buf, bytes);
        if (r == 0) {
            DPRINT("write(buf): Nothing has been written to buf.\n");
        } else if (r == -1) {
            perror("write(buf)");
        } else if (r < bytes) {
            DPRINT("write(buf): only write %zu bytes, should be %d bytes.\n", r, bytes);
        }
        current += bytes;
    }
}

int open_file(char* output_file, int frameNo, user_file_format_t ufft) {
    int ofd;
    char fileName[256];

    if (!output_file) {
        ofd = 1;
    } else {
        char *dot = strrchr(output_file, '.');
        int prefix_length;
        if (dot && !strcmp(dot+1, ufft.extension)) {
            prefix_length = dot - output_file;
        } else {
            prefix_length = strlen(output_file);
        }
        snprintf(fileName, 256, "%.*s-%04d.%s", prefix_length, output_file, frameNo, ufft.extension);
        ofd = open(fileName, FILE_ACCESS, 0664);
        if (ofd == -1) {
            fprintf(stderr, "Could not open %s\n", output_file);
            return -1;
        }
    }
    return ofd;
}

void warning_message( const char* message, ... ) {
    if ( warnings ) {
        // Write to stderr
        //
        va_list argp;
        va_start(argp, message);
        vfprintf( stderr, message, argp );
        va_end(argp);
    }
}

void process_wbadj( const char* argv0, const char chr, uint32_t adj, uint32_t *wbadj_mg, uint32_t *wbadj_ba ) {
    if ( chr == 'M' ) {
        *wbadj_mg = 7 - adj;
    } else if ( chr == 'G' )  {
        *wbadj_mg = 7 + adj;
    } else if ( chr == 'B' ) {
        *wbadj_ba = 7 - adj;
    } else if ( chr == 'A' ) {
        *wbadj_ba = 7 + adj;
    } else {
        warning_message("%s: Invalid white_balance_adjustment\n", argv0);
    }
}

void bulb_old(pslr_handle_t camhandle, pslr_rational_t shutter_speed, struct timeval prev_time) {
    DPRINT("bulb oldstyle\n");
    struct timeval current_time;
    pslr_bulb( camhandle, true );
    pslr_shutter(camhandle);
    gettimeofday(&current_time, NULL);
    double waitsec = 1.0 * shutter_speed.nom / shutter_speed.denom - timeval_diff_sec(&current_time, &prev_time);
    if ( waitsec < 0 ) {
        waitsec = 0;
    }
    sleep_sec( waitsec  );
    pslr_bulb( camhandle, false );
}

void bulb_new(pslr_handle_t camhandle, pslr_rational_t shutter_speed) {
    if (pslr_has_setting_by_name(camhandle, "bulb_timer")) {
        pslr_write_setting_by_name(camhandle, "bulb_timer", 1);
    } else if (pslr_has_setting_by_name(camhandle, "astrotracer")) {
        pslr_write_setting_by_name(camhandle, "astrotracer", 1);
    } else {
        fprintf(stderr, "New bulb mode is not supported for this camera model\n");
    }
    int bulb_sec = (int)(shutter_speed.nom / shutter_speed.denom);
    if (pslr_has_setting_by_name(camhandle, "bulb_timer_sec")) {
        pslr_write_setting_by_name(camhandle, "bulb_timer_sec", bulb_sec);
    } else if (pslr_has_setting_by_name(camhandle, "astrotracer_timer_sec")) {
        pslr_write_setting_by_name(camhandle, "astrotracer_timer_sec", bulb_sec);
    } else {
        fprintf(stderr, "New bulb mode is not supported for this camera model\n");
    }
    pslr_shutter(camhandle);
}

void bulb_new_cleanup(pslr_handle_t camhandle) {
    if (pslr_has_setting_by_name(camhandle, "bulb_timer")) {
        if (!bulb_timer_before) {
            pslr_write_setting_by_name(camhandle, "bulb_timer", bulb_timer_before);
        }
    } else if (pslr_has_setting_by_name(camhandle, "astrotracer")) {
        if (!astrotracer_before) {
            pslr_write_setting_by_name(camhandle, "astrotracer", astrotracer_before);
        }
    }
}
