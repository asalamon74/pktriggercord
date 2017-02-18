/*
    pkTriggerCord
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2011-2017 Andras Salamon <andras.salamon@melda.info>

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

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <math.h>
#include <sys/time.h>

#include "pslr.h"
//#include "pslr_lens.h"
#include "pktriggercord-servermode.h"

#ifdef WIN32
#define FILE_ACCESS O_WRONLY | O_CREAT | O_TRUNC | O_BINARY
#else
#define FILE_ACCESS O_WRONLY | O_CREAT | O_TRUNC
#endif

extern char *optarg;
extern int optind, opterr, optopt;
bool debug = false;
bool warnings = false;

const char *shortopts = "m:q:a:r:d:t:o:i:F:fghvsw";

static struct option const longopts[] = {
    {"exposure_mode", required_argument, NULL, 'm'},
    {"resolution", required_argument, NULL, 'r'},
    {"quality", required_argument, NULL, 'q'},
    {"aperture", required_argument, NULL, 'a'},
    {"shutter_speed", required_argument, NULL, 't'},
    {"iso", required_argument, NULL, 'i'},
    {"file_format", required_argument, NULL, 1},
    {"output_file", required_argument, NULL, 'o'},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"status", no_argument, NULL, 's'},
    {"status_hex", no_argument, NULL, 2},
    {"frames", required_argument, NULL, 'F'},
    {"delay", required_argument, NULL, 'd'},
    {"auto_focus", no_argument, NULL, 'f'},
    {"green", no_argument, NULL, 'g'},
    {"warnings", no_argument, NULL, 'w'},
    {"exposure_compensation", required_argument, NULL, 3},
    {"flash_exposure_compensation", required_argument, NULL, 5},
    {"debug", no_argument, NULL, 4},
    {"dust_removal", no_argument, NULL, 6},
    {"color_space", required_argument, NULL, 7},
    {"af_mode", required_argument, NULL, 8},
    {"ae_metering", required_argument, NULL, 9},
    {"flash_mode", required_argument, NULL, 10},
    {"drive_mode", required_argument, NULL, 11},
    {"select_af_point", required_argument, NULL, 12},
    {"jpeg_image_tone", required_argument, NULL, 13},
    {"white_balance_mode", required_argument, NULL, 14},
    {"white_balance_adjustment", required_argument, NULL, 15},
    {"model", required_argument, NULL, 16},
    {"nowarnings", no_argument, NULL, 17},
    {"device", required_argument, NULL, 18},
    {"reconnect", no_argument, NULL, 19},
    {"timeout", required_argument, NULL, 20},
    {"noshutter", no_argument, NULL, 21},
#ifndef WIN32
    {"servermode", no_argument, NULL, 22},
    {"servermode_timeout", required_argument, NULL, 23},
#endif
    {"pentax_debug_mode", required_argument, NULL,24},
    {"dangerous", no_argument, NULL, 25},
    { NULL, 0, NULL, 0}
};

int save_buffer(pslr_handle_t, int, int, pslr_status*, user_file_format, int);
void print_status_info(pslr_handle_t h, pslr_status status);
void usage(char*);

int open_file(char* output_file, int frameNo, user_file_format_t ufft) {
    int ofd = -1;
    char fileName[256];

    if (!output_file) {
        ofd = 1;
    } else {
        snprintf(fileName, 256, "%s-%04d.%s", output_file, frameNo, ufft.extension);
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

char *copyright_version(char *name, char *version) {
    char *ret = malloc(sizeof(char)*1024);
    sprintf(ret, "%s %s\n\n\%s\
License LGPLv3: GNU LGPL version 3 <http://gnu.org/licenses/lgpl.html>\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n", name, version, copyright() );
    return ret;
}

int main(int argc, char **argv) {
    float F = 0;
    char C;
    char c1;
    char c2;
    char *output_file = NULL;
    char *model = NULL;
    char *device = NULL;
    const char *camera_name;
    char *MODESTRING = NULL;
    int resolution = 0;
    int quality = -1;
    int optc, fd, i;
    int wbadj_ss=0;
    pslr_handle_t camhandle;
    pslr_status status;
    user_file_format uff = USER_FILE_FORMAT_MAX;
    pslr_exposure_mode_t EM = PSLR_EXPOSURE_MODE_MAX;
//    pslr_jpeg_resolution_t R = PSLR_JPEG_RESOLUTION_MAX;
    pslr_rational_t aperture = {0, 0};
    pslr_rational_t shutter_speed = {0, 0};
    uint32_t iso = 0;
    uint32_t auto_iso_min = 0;
    uint32_t auto_iso_max = 0;
    int frames = 1;
    int delay = 0;
    int timeout = 0;
    bool auto_focus = false;
    bool green = false;
    bool dust = false;
    bool status_info = false;
    bool status_hex_info = false;
    pslr_rational_t ec = {0, 0};
    pslr_rational_t fec = {0, 0};
    pslr_color_space_t color_space = -1;
    pslr_af_mode_t af_mode = -1;
    pslr_ae_metering_t ae_metering = -1;
    pslr_flash_mode_t flash_mode = -1;
    pslr_drive_mode_t drive_mode = -1;
    pslr_af_point_sel_t af_point_sel = -1;
    pslr_jpeg_image_tone_t jpeg_image_tone = -1;
    pslr_white_balance_mode_t white_balance_mode = -1;
    uint32_t white_balance_adjustment_mg = 0;
    uint32_t white_balance_adjustment_ba = 0;
    uint32_t adj1;
    uint32_t adj2;
    bool reconnect = false;
    struct timeval prev_time;
    struct timeval current_time;
    bool noshutter = false;
#ifndef WIN32
    bool servermode = false;
    int servermode_timeout = 30;
#endif

    int modify_debug_mode=0;
    char debug_mode=0;
    bool dangerous=0;

    // just parse warning, debug flags
    while  ((optc = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
        switch (optc) {
            case 'w':
                warnings = true;
                break;
            case 17:
                warnings = false;
                break;
            case 4:
                debug = true;
                DPRINT( "Debug messaging is now enabled.\n" );
                break;
        }
    }
    optind = 1;
    // parse all the other flags
    while ((optc = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
        switch (optc) {
            case '?':
            case 'h':
                usage(argv[0]);
                exit(-1);
            /***************************************************************/
            case 'v':
                printf("%s", copyright_version(argv[0], VERSION));
                exit(0);
            case 6:
                dust = true;
                break;
            case 1:
                for (i = 0; i < strlen(optarg); i++) {
                    optarg[i] = toupper(optarg[i]);
                }
                if (!strcmp(optarg, "DNG")) {
                    uff = USER_FILE_FORMAT_DNG;
                } else if (!strcmp(optarg, "PEF")) {
                    uff = USER_FILE_FORMAT_PEF;
                } else if (!strcmp(optarg, "JPEG") || !strcmp(optarg, "JPG")) {
                    uff = USER_FILE_FORMAT_JPEG;
                } else {
                    warning_message("%s: Invalid file format.\n", argv[0]);
                }
                break;

            case 's':
                status_info = true;
                break;

            case 2:
                status_hex_info = true;
                break;

            case 'm':

                MODESTRING = optarg;
                for (i = 0; i < strlen(optarg); i++) {
                    optarg[i] = toupper(optarg[i]);
                }

                if (!strcmp(optarg, "GREEN")) {
                    EM = PSLR_EXPOSURE_MODE_GREEN;
                } else if (!strcmp(optarg, "P")) {
                    EM = PSLR_EXPOSURE_MODE_P;
                } else if (!strcmp(optarg, "SV")) {
                    EM = PSLR_EXPOSURE_MODE_SV;
                } else if (!strcmp(optarg, "TV")) {
                    EM = PSLR_EXPOSURE_MODE_TV;
                } else if (!strcmp(optarg, "AV")) {
                    EM = PSLR_EXPOSURE_MODE_AV;
                } else if (!strcmp(optarg, "TAV")) {
                    EM = PSLR_EXPOSURE_MODE_TAV;
                } else if (!strcmp(optarg, "M")) {
                    EM = PSLR_EXPOSURE_MODE_M;
                } else if (!strcmp(optarg, "B")) {
                    EM = PSLR_EXPOSURE_MODE_B;
                } else if (!strcmp(optarg, "X")) {
                    EM = PSLR_EXPOSURE_MODE_X;
                } else {
                    warning_message("%s: Invalid exposure mode.\n", argv[0]);
                }
                break;

            case 'r':
                resolution = atoi(optarg);
                break;

            case 7:
                color_space = get_pslr_color_space( optarg );
                if ( color_space == -1 ) {
                    warning_message("%s: Invalid color space\n", argv[0]);
                }
                break;

            case 8:
                af_mode = get_pslr_af_mode( optarg );
                if ( af_mode == -1 || af_mode == 0 ) {
                    // 0: changing MF does not work
                    warning_message("%s: Invalid af mode\n", argv[0]);
                }
                break;

            case 9:
                ae_metering = get_pslr_ae_metering( optarg );
                if ( ae_metering == -1 ) {
                    warning_message("%s: Invalid ae metering\n", argv[0]);
                }
                break;

            case 10:
                flash_mode = get_pslr_flash_mode( optarg );
                if ( flash_mode == -1 ) {
                    warning_message("%s: Invalid flash_mode\n", argv[0]);
                }
                break;

            case 11:
                drive_mode = get_pslr_drive_mode( optarg );
                if ( drive_mode == -1 ) {
                    warning_message("%s: Invalid drive_mode\n", argv[0]);
                }
                break;

            case 12:
                af_point_sel = get_pslr_af_point_sel( optarg );
                if ( af_point_sel == -1 ) {
                    warning_message("%s: Invalid select af point\n", argv[0]);
                }
                break;

            case 13:
                jpeg_image_tone = get_pslr_jpeg_image_tone( optarg );
                if ( jpeg_image_tone == -1 ) {
                    warning_message("%s: Invalid jpeg_image_tone\n", argv[0]);
                }
                break;

            case 14:
                white_balance_mode = get_pslr_white_balance_mode( optarg );
                if ( white_balance_mode == -1 ) {
                    warning_message("%s: Invalid white_balance_mode\n", argv[0]);
                }
                break;

            case 15:
                wbadj_ss = sscanf(optarg, "%c%d%c%d%c", &c1, &adj1, &c2, &adj2, &C);
                if ( wbadj_ss == 4 || wbadj_ss == 2 ) {
                    c1 = toupper(c1);
                    process_wbadj( argv[0], c1, adj1, &white_balance_adjustment_mg, &white_balance_adjustment_ba );
                    if ( wbadj_ss == 4 ) {
                        c2 = toupper(c2);
                        process_wbadj( argv[0], c2, adj2, &white_balance_adjustment_mg, &white_balance_adjustment_ba );
                    }
                } else {
                    warning_message("%s: Invalid white_balance_adjustment\n", argv[0]);
                }
                break;

            case 16:
                model = optarg;
                break;

            case 18:
                device = optarg;
                break;

            case 19:
                reconnect = true;
                break;

            case 20:
                timeout = atoi(optarg);
                break;

            case 'q':
                quality  = atoi(optarg);
                if (!quality) {
                    warning_message("%s: Invalid jpeg quality\n", argv[0]);
                }
                break;

            case 'a':
                if (sscanf(optarg, "%f%c", &F, &C) != 1) {
                    F = 0;
                }

                /*It's unlikely that you want an f-number > 100, even for a pinhole.
                 On the other hand, the fastest lens I know of is a f:0.8 Zeiss*/

                if (F > 100 || F < 0.8) {
                    warning_message( "%s: Invalid aperture value.\n", argv[0]);
                }

                if (F >= 11) {
                    aperture.nom = F;
                    aperture.denom = 1;
                } else {
                    F = (F * 10.0);
                    aperture.nom = F;
                    aperture.denom = 10;
                }

                break;

            case 't':
                if (sscanf(optarg, "1/%d%c", &shutter_speed.denom, &C) == 1) {
                    shutter_speed.nom = 1;
                } else if ((sscanf(optarg, "%f%c", &F, &C)) == 1) {
                    if (F < 2) {
                        F = F * 10;
                        shutter_speed.denom = 10;
                        shutter_speed.nom = F;
                    } else {
                        shutter_speed.denom = 1;
                        shutter_speed.nom = F;
                    }
                } else {
                    warning_message("%s: Invalid shutter speed value.\n", argv[0]);
                }
                break;

            case 'o':
                output_file = optarg;
                break;

            case 'f':
                auto_focus = true;
                break;

            case 'g':
                green = true;
                break;

            case 'F':
                frames = atoi(optarg);
                if (frames > 9999) {
                    warning_message("%s: Invalid frame number.\n", argv[0]);
                    frames = 9999;
                }
                break;

            case 'd':
                delay = atoi(optarg);
                if (!delay) {
                    warning_message("%s: Invalid delay value\n", argv[0]);
                }
                break;

            case 'i':
                if (sscanf(optarg, "%d-%d%c", &auto_iso_min, &auto_iso_max, &C) != 2) {
                    auto_iso_min = 0;
                    auto_iso_max = 0;
                    iso = atoi(optarg);
                }
                if (iso==0 && auto_iso_min==0) {
                    warning_message("%s: Invalid iso value\n", argv[0]);
                    exit(-1);
                }
                break;

            case 3:
                if ( sscanf(optarg, "%f%c", &F, &C) == 1 ) {
                    ec.nom=10*F;
                    ec.denom=10;
                }
                break;

            case 5:
                if ( sscanf(optarg, "%f%c", &F, &C) == 1 ) {
                    fec.nom=10*F;
                    fec.denom=10;
                }
                break;

            case 21:
                noshutter = true;
                break;

#ifndef WIN32
            case 22:
                servermode = true;
                break;

            case 23:
                servermode_timeout = atoi(optarg);
                break;
#endif

            case 24:
                modify_debug_mode=1;
                debug_mode=atoi(optarg);
                break;

            case 25:
                dangerous = true;
                break;
        }
    }

#ifndef WIN32
    if ( servermode ) {
        // ignore all the other argument and go to server mode
        servermode_socket(servermode_timeout);
        exit(0);
    }
#endif

    if (!output_file && frames > 1) {
        fprintf(stderr, "Should specify output filename if frames>1\n");
        exit(-1);
    }

    DPRINT("%s %s \n", argv[0], VERSION);
    DPRINT("model %s\n", model );
    DPRINT("device %s\n", device );

    char buf[2100];

    if ( !(camhandle = camera_connect( model, device, timeout, buf)) ) {
        printf("%s", buf);
        exit(-1);
    }

    camera_name = pslr_camera_name(camhandle);
    if ( strcmp(camera_name, "K-70")==0 && !dangerous ) {
        printf("Tethering K-70 is dangerous, please check https://github.com/asalamon74/pktriggercord/issues/14\nIf you really want to use it you also need to add --dangerous to the command line.\n");
        camera_close(camhandle);
        exit(-1);
    }
    printf("%s: %s Connected...\n", argv[0], camera_name);

    /* if debug mode switch is on, there is a possibility someone just want to alter debug mode */
    if ( modify_debug_mode == 1) {
        debug_onoff(camhandle,debug_mode);
        camera_close(camhandle);
        exit(0);
    }

    pslr_get_status(camhandle, &status);

    if ( color_space != -1 ) {
        pslr_set_color_space( camhandle, color_space );
    }

    if ( af_mode != -1 ) {
        pslr_set_af_mode( camhandle, af_mode );
    }

    if ( af_point_sel != -1 ) {
        pslr_set_af_point_sel( camhandle, af_point_sel );
    }

    if ( ae_metering != -1 ) {
        pslr_set_ae_metering_mode( camhandle, ae_metering );
    }

    if ( flash_mode != -1 ) {
        pslr_set_flash_mode( camhandle, flash_mode );
    }

    if ( jpeg_image_tone != -1 ) {
        if ( jpeg_image_tone > pslr_get_model_max_supported_image_tone(camhandle) ) {
            warning_message("%s: Invalid jpeg image tone setting.\n", argv[0]);
        }
        pslr_set_jpeg_image_tone( camhandle, jpeg_image_tone );
    }

    if ( white_balance_mode != -1 ) {
        pslr_set_white_balance( camhandle, white_balance_mode );
        if ( wbadj_ss > 0 ) {
            pslr_set_white_balance_adjustment( camhandle, white_balance_mode, white_balance_adjustment_mg, white_balance_adjustment_ba );
        }
    } else if ( white_balance_mode == -1 && wbadj_ss > 0 ) {
        pslr_set_white_balance_adjustment( camhandle, status.white_balance_mode, white_balance_adjustment_mg, white_balance_adjustment_ba);
    }

    if ( drive_mode != -1 ) {
        pslr_set_drive_mode( camhandle, drive_mode );
    }

    if ( uff == USER_FILE_FORMAT_MAX ) {
        // not specified
        if ( !pslr_get_model_only_limited( camhandle ) ) {
            // use the default of the camera
            uff = get_user_file_format( &status );
        } else {
            // use PEF, since all the camera supports this
            uff = USER_FILE_FORMAT_PEF;
        }
    } else {
        // set the requested format
        pslr_set_user_file_format(camhandle, uff);
    }

    if (resolution) {
        pslr_set_jpeg_resolution(camhandle, resolution);
    }

    if (quality>-1) {
        if ( quality > pslr_get_model_max_jpeg_stars(camhandle) ) {
            warning_message("%s: Invalid jpeg quality setting.\n", argv[0]);
        }
        pslr_set_jpeg_stars(camhandle, quality);
    }

    // We do not check iso settings
    // The camera can handle invalid iso settings (it will use ISO 800 instead of ISO 795)

    if (EM != PSLR_EXPOSURE_MODE_MAX) {
        pslr_set_exposure_mode(camhandle, EM);
    }

    if ( ec.denom ) {
        pslr_set_ec( camhandle, ec );
    }

    if ( fec.denom ) {
        pslr_set_flash_exposure_compensation( camhandle, fec );
    }

    if (iso >0 || auto_iso_min >0) {
        pslr_set_iso(camhandle, iso, auto_iso_min, auto_iso_max);
    }

    /* For some reason, resolution is not set until we read the status: */
    pslr_get_status(camhandle, &status);

    if ( quality == -1 ) {
        // quality is not set we read it from the camera
        quality = status.jpeg_quality;
    }

    if (EM != PSLR_EXPOSURE_MODE_MAX && status.exposure_mode != EM) {
        warning_message( "%s: Cannot set %s mode; set the mode dial to %s or USER\n", argv[0], MODESTRING, MODESTRING);
    }

    if (shutter_speed.nom) {
        DPRINT("shutter_speed.nom=%d\n", shutter_speed.nom);
        DPRINT("shutter_speed.denom=%d\n", shutter_speed.denom);

        if (shutter_speed.nom <= 0 || (shutter_speed.nom > 30 && status.exposure_mode != PSLR_GUI_EXPOSURE_MODE_B ) || shutter_speed.denom <= 0 || shutter_speed.denom > pslr_get_model_fastest_shutter_speed(camhandle)) {
            warning_message("%s: Invalid shutter speed value.\n", argv[0]);
        }

        pslr_set_shutter(camhandle, shutter_speed);
    } else if ( status.exposure_mode == PSLR_GUI_EXPOSURE_MODE_B ) {
        warning_message("%s: Shutter speed not specified in Bulb mode. Using 30s.\n", argv[0]);
        shutter_speed.nom = 30;
        shutter_speed.denom = 1;
    }

    if (aperture.nom) {
        if ((aperture.nom * status.lens_max_aperture.denom) > (aperture.denom * status.lens_max_aperture.nom)) {
            warning_message("%s: Warning, selected aperture is smaller than this lens minimum aperture.\n", argv[0]);
            warning_message("%s: Setting aperture to f:%d\n", argv[0], status.lens_max_aperture.nom / status.lens_max_aperture.denom);
        }

        if ((aperture.nom * status.lens_min_aperture.denom) < (aperture.denom * status.lens_min_aperture.nom)) {
            warning_message( "%s: Warning, selected aperture is wider than this lens maximum aperture.\n", argv[0]);
            warning_message( "%s: Setting aperture to f:%.1f\n", argv[0], (float) status.lens_min_aperture.nom / (float) status.lens_min_aperture.denom);
        }


        pslr_set_aperture(camhandle, aperture);
    }

    int frameNo;

    if (auto_focus) {
        pslr_focus(camhandle);
    }

    if (green) {
        pslr_green_button( camhandle );
    }

//    pslr_test( camhandle, true, 0x1e, 4, 1, 2, 3, 4);
//    pslr_button_test( camhandle, 0x0c, 1 );
//    pslr_button_test( camhandle, 0x05, 2 );
//    sleep_sec(3);
//    pslr_button_test( camhandle, 0x0c, 0 );

    // read the status after the settings
    pslr_get_status(camhandle, &status);

    if ( status_hex_info || status_info ) {
        if ( status_hex_info ) {
            int bufsize = pslr_get_model_buffer_size( camhandle );
            uint8_t status_buffer[MAX_STATUS_BUF_SIZE];
            pslr_get_status_buffer(camhandle, status_buffer);
            hexdump( status_buffer, bufsize > 0 ? bufsize : MAX_STATUS_BUF_SIZE);
        }
        print_status_info( camhandle, status );
        camera_close(camhandle);
        exit(0);
    }

    if ( dust ) {
        pslr_dust_removal(camhandle);
        camera_close(camhandle);
        exit(0);
    }

    double waitsec=0;
    user_file_format_t ufft = *get_file_format_t(uff);
    int bracket_count = status.auto_bracket_picture_count;
    if ( bracket_count < 1 || status.auto_bracket_mode == 0 ) {
        bracket_count = 1;
    }
    gettimeofday(&prev_time, NULL);

    int bracket_index=0;
    int buffer_index;

    bool continuous = status.drive_mode == PSLR_DRIVE_MODE_CONTINUOUS_HI ||
                      status.drive_mode == PSLR_DRIVE_MODE_CONTINUOUS_LO;
    DPRINT("cont: %d\n", continuous);

    for (frameNo = 0; frameNo < frames; ++frameNo) {
        gettimeofday(&current_time, NULL);
        if ( bracket_count <= bracket_index ) {
            if ( reconnect ) {
                camera_close( camhandle );
                while (!(camhandle = pslr_init( model, device ))) {
                    sleep_sec(1);
                }
                pslr_connect(camhandle);
            }
            waitsec = 1.0 * delay - timeval_diff(&current_time, &prev_time) / 1000000.0;
            if ( waitsec > 0 ) {
                printf("Waiting for %.2f sec\n", waitsec);
                sleep_sec( waitsec );
            }
            bracket_index = 0;
            gettimeofday(&prev_time, NULL);
        }
        if ( noshutter ) {
            while (1) {
                if ( PSLR_OK != pslr_get_status (camhandle, &status) ) {
                    break;
                }

                if ( status.bufmask != 0 ) {
                    break; //new image ?
                }

                gettimeofday (&current_time, NULL);
                if ( timeout != 0 && ( timeval_diff(&current_time, &prev_time) / 1000000.0 >= timeout) ) {
                    printf("Timeout %d sec passed!\n", timeout);
                    break;
                }

                usleep(100000); /* 100 ms */
            }
        } else {
            if ( frames > 1 ) {
                printf("Taking picture %d/%d\n", frameNo+1, frames);
                fflush(stdout);
            }
            if ( status.exposure_mode ==  PSLR_GUI_EXPOSURE_MODE_B ) {
                DPRINT("bulb\n");
                pslr_bulb( camhandle, true );
                pslr_shutter(camhandle);
                gettimeofday(&current_time, NULL);
                waitsec = 1.0 * shutter_speed.nom / shutter_speed.denom - timeval_diff(&current_time, &prev_time) / 1000000.0;
                if ( waitsec < 0 ) {
                    waitsec = 0;
                }
                sleep_sec( waitsec  );
                pslr_bulb( camhandle, false );
            } else {
                DPRINT("not bulb\n");
                pslr_shutter(camhandle);
            }
            pslr_get_status(camhandle, &status);
        }
        if ( bracket_index+1 >= bracket_count || frameNo+1>=frames ) {
            if ( bracket_index+1 < bracket_count ) {
                // partial bracket set
                bracket_count = bracket_index+1;
            }
            for ( buffer_index = 0; buffer_index < bracket_count; ++buffer_index ) {
                fd = open_file(output_file, frameNo-bracket_count+buffer_index+1, ufft);
                while ( save_buffer(camhandle, buffer_index, fd, &status, uff, quality) ) {
                    usleep(10000);
                }
                pslr_delete_buffer(camhandle, buffer_index);
                if (fd != 1) {
                    close(fd);
                }
            }
        }
        ++bracket_index;
    }
    camera_close(camhandle);

    exit(0);
}

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
        return (1);
    }

    length = pslr_buffer_get_size(camhandle);
    DPRINT("Buffer length: %d\n", length);
    current = 0;

    while (1) {
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
            DPRINT("write(buf): only write %d bytes, should be %d bytes.\n", r, bytes);
        }
        current += bytes;
    }
    pslr_buffer_close(camhandle);
    return (0);
}

void print_status_info( pslr_handle_t h, pslr_status status ) {
    printf("\n");
    printf( "%s", collect_status_info( h, status ) );
}

void usage(char *name) {
    printf("\nUsage: %s [OPTIONS]\n\n\
Shoot a Pentax DSLR and send the picture to standard output.\n\
\n\
      --model=CAMERA_MODEL              valid values are: K20d, K10d, GX10, GX20, K-x, K200D, K-7, K-r, K-5, K-2000, K-m, K-30, K100D, K110D, K-01, K-3, K-3II, K-500\n\
      --device=DEVICE                   valid values for Linux: sg0, sg1, ..., for Windows: C, D, E, ...\n\
      --timeout=SECONDS                 timeout for camera connection ( 0 means forever )\n\
  -w, --warnings                        warning mode on\n\
      --nowarnings                      warning mode off\n\
  -m, --exposure_mode=MODE              valid values are GREEN, P, SV, TV, AV, TAV, M and X\n\
      --exposure_compensation=VALUE     exposure compensation value\n\
      --drive_mode=DRIVE_MODE           valid values are: Single, Continuous-HI, SelfTimer-12, SelfTimer-2, Remote, Remote-3, Continuous-LO\n\
  -i, --iso=ISO                         single value (400) or interval (200-800)\n\
      --color_space=COLOR_SPACE         valid values are: sRGB, AdobeRGB\n\
      --af_mode=AF_MODE                 valid values are: AF.S, AF.C, AF.A\n\
      --select_af_point=AF_SELECT_MODE  valid values are: Auto-5, Auto-11, Spot, Select\n\
      --ae_metering=AE_METERING         valid values are: Multi, Center, Spot\n\
      --flash_mode=FLASH_MODE           valid values are: Manual, Manual-RedEye, Slow, Slow-RedEye, TrailingCurtain, Auto, Auto-RedEye, Wireless\n\
      --flash_exposure_compensation=VAL flash exposure compensation value\n\
  -a, --aperture=APERTURE\n\
  -t, --shutter_speed=SHUTTER SPEED     values can be given in rational form (eg. 1/90) or decimal form (eg. 0.8)\n\
  -r, --resolution=RESOLUTION           resolution in megapixels\n\
  -q, --quality=QUALITY                 valid values are 1, 2, 3 and 4\n\
      --jpeg_image_tone=IMAGE_TONE      valid values are: Natural, Bright, Portrait, Landscape, Vibrant, Monochrome, Muted, ReversalFilm\n\
      --white_balance_mode=WB_MODE      valid values are: Auto, Daylight, Shade, Cloudy, Fluorescent_D, Fluorescent_N, Fluorescent_W, Fluorescent_L, Tungsten, Flash, Manual, CTE\n\
      --white_balance_adjustment=WB_ADJ valid values like: G5B2, G3A5, B5, A3, G5, M4...\n\
  -f, --auto_focus                      autofocus\n\
      --reconnect                       reconnect between shots\n\
      --servermode                      start in server mode and wait for commands\n\
      --servermode_timeout=SECONDS      servermode timeout\n\
  -g, --green                           green button\n\
  -s, --status                          print status info\n\
      --status_hex                      print status hex info\n\
      --dust_removal                    dust removal\n\
  -F, --frames=NUMBER                   number of frames\n\
  -d, --delay=SECONDS                   delay between the frames (seconds)\n\
      --file_format=FORMAT              valid values: PEF, DNG, JPEG\n\
  -o, --output_file=FILE                send output to FILE instead of stdout\n\
      --debug                           turn on debug messages\n\
      --noshutter                       do not send shutter command, just wait for new photo, download and delete from camera\n\
  -v, --version                         display version information and exit\n\
  -h, --help                            display this help and exit\n\
      --pentax_debug_mode={0|1}		enable or disable camera debug mode and exit (DANGEROUS). Valid values are: 0, 1\n\
\n", name);
}
