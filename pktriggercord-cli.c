/*
    pkTriggerCord
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2011 Andras Salamon <andras.salamon@melda.info>

    based on:

    pslr-shoot

    Command line remote control of Pentax DSLR cameras.
    Copyright (C) 2009 Ramiro Barreiro <ramiro_barreiro69@yahoo.es>
    With fragments of code from PK-Remote by Pontus Lidman. 
    <https://sourceforge.net/projects/pkremote>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3 as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/gpl.html>.
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

#include "pslr.h"

extern char *optarg;
extern int optind, opterr, optopt;

static struct option const longopts[] ={
    {"exposure_mode", required_argument, NULL, 'm'},
    {"resolution", required_argument, NULL, 'r'},
    {"quality", required_argument, NULL, 'q'},
    {"aperture", required_argument, NULL, 'a'},
    {"shutter_speed", required_argument, NULL, 't'},
    {"iso", required_argument, NULL, 'i'},
    {"output_file", required_argument, NULL, 'o'},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"status", no_argument, NULL, 's'},
    {"frames", required_argument, NULL, 'F'},
    {"delay", required_argument, NULL, 'd'},
    {"auto_focus", no_argument, NULL, 'f'},
    { NULL, 0, NULL, 0}
};

int save_buffer(pslr_handle_t, int, int, pslr_status*);
void print_status_info(pslr_status status);
void usage(char*);
void version(char*);
void CLOSE(pslr_handle_t, int);

int open_file(char* output_file, int frameNo) {
    int ofd = -1;
    char fileName[256];

    if (!output_file) {
        ofd = 1;
    } else {
        snprintf(fileName, 256, "%s-%04d.dng", output_file, frameNo);
        ofd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0664);
        if (ofd == -1) {
            fprintf(stderr, "Could not open %s\n", output_file);
            return -1;
        }
    }
    return ofd;
}

int main(int argc, char **argv) {
    float F = 0;
    char C;
    char *output_file = NULL;
    const char *MODEL;
    char *MODESTRING = NULL;
    char *resolution = NULL;
    char *quality = NULL;
    int optc, fd, i;
    pslr_handle_t camhandle;
    pslr_status status;
    pslr_exposure_mode_t EM = PSLR_EXPOSURE_MODE_MAX;
    pslr_jpeg_resolution_t R = PSLR_JPEG_RESOLUTION_MAX;
    pslr_jpeg_quality_t Q = PSLR_JPEG_QUALITY_MAX;
    pslr_rational_t aperture = {0, 0};
    pslr_rational_t shutter_speed = {0, 0};
    uint32_t iso = 0;
    bool IS_K20D = false;
    int frames = 1;
    int delay = 0;
    bool auto_focus = false;
    bool status_info = false;

    while ((optc = getopt_long(argc, argv, "m:q:a:d:t:o:i:F:fhvs", longopts, NULL)) != -1) {
        switch (optc) {
                /***************************************************************/
            case '?': case 'h':
                usage(argv[0]);
                exit(-1);
                /***************************************************************/
            case 'v':
                version(argv[0]);
                exit(0);
            case 's':
                status_info = true;
                break;

                /***************************************************************/
            case 'm':

                MODESTRING = optarg;
                for (i = 0; i < strlen(optarg); i++) optarg[i] = toupper(optarg[i]);

                DPRINT("mode=%s\n", optarg);

                if (!strcmp(optarg, "GREEN")) {
                    EM = PSLR_EXPOSURE_MODE_GREEN;
                    break;
                }
                else if (!strcmp(optarg, "P")) {
                    EM = PSLR_EXPOSURE_MODE_P;
                    break;
                }
                else if (!strcmp(optarg, "SV")) {
                    EM = PSLR_EXPOSURE_MODE_SV;
                    break;
                }
                else if (!strcmp(optarg, "TV")) {
                    EM = PSLR_EXPOSURE_MODE_TV;
                    break;
                }
                else if (!strcmp(optarg, "AV")) {
                    EM = PSLR_EXPOSURE_MODE_AV;
                    break;
                }
                else if (!strcmp(optarg, "TAV")) {
                    EM = PSLR_EXPOSURE_MODE_TAV;
                    break;
                }
                else if (!strcmp(optarg, "M")) {
                    EM = PSLR_EXPOSURE_MODE_M;
                    break;
                }
                else if (!strcmp(optarg, "X")) {
                    EM = PSLR_EXPOSURE_MODE_X;
                    break;
                }
                else {
                    fprintf(stderr, "%s: Invalid exposure mode.\n", argv[0]);
                    exit(-1);
                }

                /*****************************************/
            case 'r':

                DPRINT("resolution=%s\n", optarg);
                resolution = optarg;
                break;
                /* Valid resolution values depend on camera model, so we'll check it later */

                /*********************************************/
            case 'q':

                DPRINT("quality=%s\n", optarg);
                quality = optarg;
                break;
                /* Valid quality values depend on camera model, so we'll check it later */

                /****************************************************/
            case 'a':

                if (sscanf(optarg, "%f%c", &F, &C) != 1) F = 0;

                /*It's unlikely that you want an f-number > 100, even for a pinhole.
                 On the other hand, the fastest lens I know of is a f:0.8 Zeiss*/

                if (F > 100 || F < 0.8) {
                    fprintf(stderr, "%s: Invalid aperture value.\n", argv[0]);
                    exit(-1);
                }

                if (F >= 11) {
                    aperture.nom = F;
                    aperture.denom = 1;
                } else {
                    F = (F * 10.0);
                    aperture.nom = F;
                    aperture.denom = 10;
                }

                DPRINT("aperture.nom=%d\n", aperture.nom);
                DPRINT("aperture.denom=%d\n", aperture.denom);

                break;

                /*****************************************************/
            case 't':

                if (sscanf(optarg, "1/%d%c", &shutter_speed.denom, &C) == 1) shutter_speed.nom = 1;

                else if ((sscanf(optarg, "%f%c", &F, &C)) == 1) {
                    if (F < 2) {
                        F = F * 10;
                        shutter_speed.denom = 10;
                        shutter_speed.nom = F;
                    } else {
                        shutter_speed.denom = 1;
                        shutter_speed.nom = F;
                    }
                }
                else {
                    fprintf(stderr, "%s: Invalid shutter speed value.\n", argv[0]);
                    exit(-1);
                }

                DPRINT("shutter_speed.nom=%d\n", shutter_speed.nom);
                DPRINT("shutter_speed.denom=%d\n", shutter_speed.denom);

                if (shutter_speed.nom <= 0 || shutter_speed.nom > 30 || shutter_speed.denom <= 0 || shutter_speed.denom > 4000) {
                    fprintf(stderr, "%s: Invalid shutter speed value.\n", argv[0]);
                    exit(-1);
                }
                break;

                /*******************************************************/
            case 'o':

                output_file = optarg;
                break;

            case 'f':
                auto_focus = true;
                break;

            case 'F':
                frames = atoi(optarg);
                if (frames > 9999) {
                    fprintf(stderr, "%s: Invalid frame number.\n", argv[0]);
                    exit(-1);
                }
                break;

            case 'd':
                delay = atoi(optarg);
                if (!delay) {
                    fprintf(stderr, "%s: Invalid delay value\n", argv[0]);
                    exit(-1);
                }
                break;

            case 'i':

                iso = atoi(optarg);
                DPRINT("iso=%d\n", iso);
                if (!iso) {
                    fprintf(stderr, "%s: Invalid iso value\n", argv[0]);
                    exit(-1);
                }
		break;
        }
        /********************************************************/
    }

    if (!output_file && frames > 1) {
        fprintf(stderr, "Should specify output filename if frames>1\n");
        exit(-1);
    }

    while (!(camhandle = pslr_init())) sleep(1);

    if (camhandle) pslr_connect(camhandle);

    MODEL = pslr_camera_name(camhandle);
    if (!strcmp(MODEL, "K20D")) IS_K20D = true;

    printf("%s: %s Connected...\n", argv[0], MODEL);

    pslr_set_image_format(camhandle, PSLR_IMAGE_FORMAT_RAW);

    if (resolution) {
        if (!strcmp(resolution, "14")) R = PSLR_JPEG_RESOLUTION_14M;
        else if (!strcmp(resolution, "10")) R = PSLR_JPEG_RESOLUTION_10M;
        else if (!strcmp(resolution, "6")) R = PSLR_JPEG_RESOLUTION_6M;
        else if (!strcmp(resolution, "2")) R = PSLR_JPEG_RESOLUTION_2M;

        if (R == PSLR_JPEG_RESOLUTION_MAX || (!IS_K20D && R == PSLR_JPEG_RESOLUTION_14M)) {
            if (IS_K20D) fprintf(stderr, "%s: Valid resolution values are 14, 10, 6 and 2.\n", argv[0]);
            else fprintf(stderr, "%s: Valid resolution values are 10, 6 and 2.\n", argv[0]);
        }
        pslr_set_jpeg_resolution(camhandle, R);
    }


    if (quality) {
        if (!strcmp(quality, "4")) Q = PSLR_JPEG_QUALITY_4;
        else if (!strcmp(quality, "3")) Q = PSLR_JPEG_QUALITY_3;
        else if (!strcmp(quality, "2")) Q = PSLR_JPEG_QUALITY_2;
        else if (!strcmp(quality, "1")) Q = PSLR_JPEG_QUALITY_1;

        if (Q == PSLR_JPEG_QUALITY_MAX || (!IS_K20D && Q == PSLR_JPEG_QUALITY_4)) {
            if (IS_K20D) fprintf(stderr, "%s: Valid quality values are 4, 3, 2 and 1.\n", argv[0]);
            else fprintf(stderr, "%s: Valid quality values are 3, 2 and 1.\n", argv[0]);
        }
        pslr_set_jpeg_quality(camhandle, Q);
    }


    // We do not check iso settings
    // The camera can handle invalid iso settings (it will use ISO 800 instead of ISO 795)

    if (EM != PSLR_EXPOSURE_MODE_MAX) pslr_set_exposure_mode(camhandle, EM);

    if (iso) pslr_set_iso(camhandle, iso);

    /* For some reason, resolution is not set until we read the status: */
    pslr_get_status(camhandle, &status);

//    if (EM != PSLR_EXPOSURE_MODE_MAX && status.exposure_mode != EM) {
//        fprintf(stderr, "%s: Cannot set %s mode; set the mode dial to %s or USER\n", argv[0], MODESTRING, MODESTRING);
//    }

    if (shutter_speed.nom) pslr_set_shutter(camhandle, shutter_speed);

    if (aperture.nom) {
        if ((aperture.nom * status.lens_max_aperture.denom) > (aperture.denom * status.lens_max_aperture.nom)) {
            fprintf(stderr, "%s: Warning, selected aperture is smaller than this lens minimum aperture.\n", argv[0]);
            fprintf(stderr, "%s: Setting aperture to f:%d\n", argv[0], status.lens_max_aperture.nom / status.lens_max_aperture.denom);
        }

        if ((aperture.nom * status.lens_min_aperture.denom) < (aperture.denom * status.lens_min_aperture.nom)) {
            fprintf(stderr, "%s: Warning, selected aperture is wider than this lens maximum aperture.\n", argv[0]);
            fprintf(stderr, "%s: Setting aperture to f:%.1f\n", argv[0], (float) status.lens_min_aperture.nom / (float) status.lens_min_aperture.denom);
        }


        pslr_set_aperture(camhandle, aperture);


    }

    int frameNo;

    if (auto_focus) {
        pslr_focus(camhandle);
    }
    
    if( status_info ) {
	pslr_get_status(camhandle, &status);
	print_status_info( status );
	exit(0);
    }

    time_t prev_time=0;
    time_t current_time=0;
    long long int waitsec=0;
    for (frameNo = 0; frameNo < frames; ++frameNo) {
        fd = open_file(output_file, frameNo);
	prev_time=current_time;
	current_time = time(NULL);
	if( frameNo > 0 ) {
	    waitsec = delay-((long long int)current_time-(long long int)prev_time);
	    if( waitsec > 0 ) {
		printf("Waiting for %lld sec\n", waitsec);	   
		sleep( waitsec );
	    }
	}
	current_time = time(NULL);
        pslr_shutter(camhandle);
        pslr_get_status(camhandle, &status);
        while (save_buffer(camhandle, (int) 0, fd, &status)) usleep(10000);
        pslr_delete_buffer(camhandle, (int) 0);
        if (fd != 1) {
            close(fd);
        }
    }
    CLOSE(camhandle, 0);

    exit(0);

}

int save_buffer(pslr_handle_t camhandle, int bufno, int fd, pslr_status *status) {

    pslr_buffer_type imagetype;
    uint8_t buf[65536];
    uint32_t length;
    uint32_t current;

    imagetype = PSLR_BUF_DNG;

    DPRINT("get buffer %d type %d res %d\n", bufno, imagetype, status->jpeg_resolution);

    if (pslr_buffer_open(camhandle, bufno, imagetype, status->jpeg_resolution) != PSLR_OK) return (1);

    length = pslr_buffer_get_size(camhandle);
    current = 0;

    while (1) {
        uint32_t bytes;
        bytes = pslr_buffer_read(camhandle, buf, sizeof (buf));
        if (bytes == 0)
            break;
        write(fd, buf, bytes);
        current += bytes;
    }
    pslr_buffer_close(camhandle);
    return (0);
}

void CLOSE(pslr_handle_t camhandle, int exit_value) {
    pslr_disconnect(camhandle);
    pslr_shutdown(camhandle);
    exit(exit_value);
}

void print_status_info( pslr_status status ) {    
    // TODO: print other values as well
    printf("\ncurrent_iso: %d\n", status.current_iso);
    printf("current_shutter_speed: %d/%d\n", status.current_shutter_speed.nom, status.current_shutter_speed.denom);
    printf("set_iso: %d\n", status.set_iso);
    printf("exposure_mode: %d\n", status.exposure_mode);
    printf("user_mode: %d\n", status.user_mode_flag);
}

void usage(char *name) {
    printf("\nUsage: %s [OPTIONS]\n\n\
Shoot a Pentax DSLR and send the picture to standard output.\n\
\n\
  -m, --exposure_mode=MODE		valid values are GREEN, P, SV, TV, AV, TAV, M and X\n\
  -i, --iso=ISO\n\
  -a, --aperture=APERTURE\n\
  -t, --shutter_speed=SHUTTER SPEED	values can be given in rational form (eg. 1/90)\n\
					or decimal form (eg. 0.8)\n\
  -r, --resolution=RESOLUTION		valid values are 2, 6 and 10\n\
  -q, --quality=QUALITY			valid values are 1, 2 and 3\n\
  -f, --auto_focus			autofocus\n\
  -s, --status			        print status info\n\
  -F, --frames=NUMBER			number of frames\n\
  -d, --delay=SECONDS			delay between the frames (seconds)\n\
  -o, --output_file=FILE		send output to FILE instead of stdout\n\
  -v, --version				display version information and exit\n\
  -h, --help				display this help and exit\n\
\n", name);
}

void version(char *name) {
    printf("\n%s 0.70.00\n\n\
Copyright (C) 2011 Andras Salamon\n\
License GPLv3: GNU GPL version 3 <http://gnu.org/licenses/gpl.html>\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n\
\n\
Based on:\n\
pslr-shoot (C) 2009 Ramiro Barreiro\n\
PK-Remote (C) 2008 Pontus Lidman \n\n", name);
}

