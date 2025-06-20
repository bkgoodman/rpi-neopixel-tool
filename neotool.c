/*
 * newtest.c
 *
 * Copyright (c) 2014 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


static char VERSION[] = "XX.YY.ZZ";

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <math.h>


#include <errno.h>
#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"
#include "version.h"

#include "ws2811.h"


#define ARRAY_SIZE(stuff)       (sizeof(stuff) / sizeof(stuff[0]))

// defaults for cmdline options
#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN                18
#define DMA                     10
//#define STRIP_TYPE            WS2811_STRIP_RGB		// WS2812/SK6812RGB integrated chip+leds
#define STRIP_TYPE              WS2811_STRIP_GBR		// WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE            SK6812_STRIP_RGBW		// SK6812RGBW (NOT SK6812RGB)

#define LED_COUNT               16
extern int optind;

int led_count = LED_COUNT;

unsigned long udelay = 100000;
int pipe_fd = -1;
char *pipename = 0L;
int clear_on_exit = 0;
char *messagestr = 0L;

ws2811_t ledstring =
{
    .freq = TARGET_FREQ,
    .dmanum = DMA,
    .channel =
    {
        [0] =
        {
            .gpionum = GPIO_PIN,
            .invert = 0,
            .count = LED_COUNT,
            .strip_type = STRIP_TYPE,
            .brightness = 255,
        },
        [1] =
        {
            .gpionum = 0,
            .invert = 0,
            .count = 0,
            .brightness = 0,
        },
    },
};

ws2811_led_t *matrix,*cmdbuf;
int cmdbuf_len;

static uint8_t running = 1;

void matrix_render(void)
{
    int i;

    for (i = 0; i < led_count; i++)
    {
       ledstring.channel[0].leds[i] = matrix[i];
    }
}

void matrix_shift(void)
{
    int i;

    for (i = 1; i < led_count; i++)
    {
        matrix[i] = matrix[i-1];
    }
    
}

typedef enum enimateMode_e {
	ANIM_NORMAL=0,
	ANIM_FADE,
	ANIM_BLINK,
	ANIM_WALK,
	ANIM_WINK,
	ANIM_SIN
} animateMode_t;

unsigned long animphase=0; // Animation Phase

animateMode_t anim = ANIM_NORMAL;

void matrix_clear(void)
{
    int i;

    for (i=0;i<led_count;i++)
    {
            matrix[i] = 0;
    }
}

int dotspos[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
ws2811_led_t dotcolors[] =
{
    0x00200000,  // red
    0x00201000,  // orange
    0x00202000,  // yellow
    0x00002000,  // green
    0x00002020,  // lightblue
    0x00000020,  // blue
    0x00100010,  // purple
    0x00200010,  // pink
};

ws2811_led_t dotcolors_rgbw[] =
{
    0x00200000,  // red
    0x10200000,  // red + W
    0x00002000,  // green
    0x10002000,  // green + W
    0x00000020,  // blue
    0x10000020,  // blue + W
    0x00101010,  // white
    0x10101010,  // white + W

};

void matrix_bottom(void)
{
    int i;

    for (i = 0; i < led_count; i++)
    {
        dotspos[i]++;
        if (dotspos[i] > 8)
        {
            dotspos[i] = 0;
        }

        if (ledstring.channel[0].strip_type == SK6812_STRIP_RGBW) {
            matrix[i] = dotcolors_rgbw[i];
        } else {
            matrix[i] = dotcolors[i];
        }
    }
}

static void ctrl_c_handler(int signum)
{
	(void)(signum);
    running = 0;
}

static void setup_handlers(void)
{
    struct sigaction sa =
    {
        .sa_handler = ctrl_c_handler,
    };

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}


void parseargs(int argc, char **argv, ws2811_t *ws2811)
{
	int index;
	int c;

	static struct option longopts[] =
	{
		{"help", no_argument, 0, 'h'},
		{"dma", required_argument, 0, 'd'},
		{"pipe", required_argument, 0, 'p'},
		{"message", required_argument, 0, 'm'},
		{"gpio", required_argument, 0, 'g'},
		{"invert", no_argument, 0, 'i'},
		{"clear", no_argument, 0, 'c'},
		{"strip", required_argument, 0, 's'},
		{"width", required_argument, 0, 'x'},
		{"version", no_argument, 0, 'v'},
		{0, 0, 0, 0}
	};

	while (1)
	{

		index = 0;
		c = getopt_long(argc, argv, "m:p:cd:g:his:vx:y:", longopts, &index);

		if (c == -1)
			break;

		switch (c)
		{
		case 0:
			/* handle flag options (array's 3rd field non-0) */
			break;

		case 'h':
			fprintf(stderr, "%s version %s\n", argv[0], VERSION);
			fprintf(stderr, "Usage: %s \n"
				"-h (--help)    - this information\n"
				"-s (--strip)   - strip type - rgb, grb, gbr, rgbw\n"
				"-x (--width)   - led count (default 8)\n"
				"-d (--dma)     - dma channel to use (default 10)\n"
				"-g (--gpio)    - GPIO to use\n"
				"                 If omitted, default is 18 (PWM0)\n"
				"-i (--invert)  - invert pin output (pulse LOW)\n"
				"-c (--clear)   - clear matrix on exit.\n"
				"-p (--pipe)    - Shared pipe filename for text updates\n"
				"-m (--message) - Pipe Message string\n"
				"-v (--version) - version information\n"
				, argv[0]);
			exit(-1);

		case 'D':
			break;

		case 'g':
			if (optarg) {
				int gpio = atoi(optarg);
/*
	PWM0, which can be set to use GPIOs 12, 18, 40, and 52.
	Only 12 (pin 32) and 18 (pin 12) are available on the B+/2B/3B
	PWM1 which can be set to use GPIOs 13, 19, 41, 45 and 53.
	Only 13 is available on the B+/2B/PiZero/3B, on pin 33
	PCM_DOUT, which can be set to use GPIOs 21 and 31.
	Only 21 is available on the B+/2B/PiZero/3B, on pin 40.
	SPI0-MOSI is available on GPIOs 10 and 38.
	Only GPIO 10 is available on all models.

	The library checks if the specified gpio is available
	on the specific model (from model B rev 1 till 3B)

*/
				ws2811->channel[0].gpionum = gpio;
			}
			break;

		case 'i':
			ws2811->channel[0].invert=1;
			break;

		case 'c':
			clear_on_exit=1;
			break;

		case 'm':
			if (optarg) {
				messagestr = strdup(optarg);
			}
			break;
		case 'd':
			if (optarg) {
				int dma = atoi(optarg);
				if (dma < 14) {
					ws2811->dmanum = dma;
				} else {
					printf ("invalid dma %d\n", dma);
					exit (-1);
				}
			}
			break;

		case 'x':
			if (optarg) {
				led_count = atoi(optarg);
				if (led_count > 0) {
					ws2811->channel[0].count = led_count;
				} else {
					printf ("invalid width %d\n", led_count);
					exit (-1);
				}
			}
			break;

		case 's':
			if (optarg) {
				if (!strncasecmp("rgb", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_RGB;
				}
				else if (!strncasecmp("rbg", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_RBG;
				}
				else if (!strncasecmp("grb", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_GRB;
				}
				else if (!strncasecmp("gbr", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_GBR;
				}
				else if (!strncasecmp("brg", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_BRG;
				}
				else if (!strncasecmp("bgr", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_BGR;
				}
				else if (!strncasecmp("rgbw", optarg, 4)) {
					ws2811->channel[0].strip_type = SK6812_STRIP_RGBW;
				}
				else if (!strncasecmp("grbw", optarg, 4)) {
					ws2811->channel[0].strip_type = SK6812_STRIP_GRBW;
				}
				else {
					printf ("invalid strip %s\n", optarg);
					exit (-1);
				}
			}
			break;

		case 'p':
			if (optarg) {
				pipename = strdup(optarg);
			}
			break;
		case 'v':
			fprintf(stderr, "%s version %s\n", argv[0], VERSION);
			exit(-1);

		case '?':
			/* getopt_long already reported error? */
			exit(-1);

		default:
			exit(-1);
		}
	}
}

void strtocmdbuf(char *str) {
	cmdbuf_len = 0;
	char *tok;
	if (cmdbuf) {
		free((void *) cmdbuf);
		cmdbuf = 0L;
	}

	// Buffer size isn't perfect, but good enough
    cmdbuf = malloc(sizeof(ws2811_led_t) * (strlen(str)/6));
    tok  = strtok(str," ");
    while (tok) {
	 if (tok[0] == '!') {
		 // If it's a delay command, set delay
		 udelay = strtoul(&tok[1],0L,10);
	 } else if (tok[0] == '@') {
		 // If it's an animation command, set the mode
		 if (tok[1] == 'q') {
			 running = 0;
		 }  else {
		 anim = tok[1] - '0';
			int flags = fcntl(pipe_fd, F_GETFL, 0);
		 if (anim != ANIM_NORMAL) {
			fcntl(pipe_fd, F_SETFL, flags | O_NONBLOCK);
		 } else {
			fcntl(pipe_fd, F_SETFL, flags & ~O_NONBLOCK);
		 }
			flags = fcntl(pipe_fd, F_GETFL, 0);
		 }
	 } else {
		 // It's an RGB color - add to buffer
		 cmdbuf[cmdbuf_len] = strtoul(tok,0L,16);
		 cmdbuf_len++;
	 }
	 tok = strtok(0L," ");
    }
}
#define SINE_WAVE_FREQUENCY 1.0f
#define SINE_WAVE_AMPLITUDE 0.5f
#define SINE_WAVE_OFFSET    0.5f
const float TWO_PI = 2.0f * M_PI;
const float FREQ_FACTOR = SINE_WAVE_FREQUENCY * TWO_PI;
const float ANIM_PHASE_SCALE = 0.4f * TWO_PI; // This part depends on animphase increment

int main(int argc, char *argv[])
{
    ws2811_return_t ret;

    sprintf(VERSION, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);

    parseargs(argc, argv, &ledstring);

    matrix = malloc(sizeof(ws2811_led_t) * led_count);
    cmdbuf = 0L;
    cmdbuf_len = 0;

    setup_handlers();

    if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS)
    {
        fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
        return ret;
    }

	if (pipename) {
		if (mkfifo(pipename, 0666) == -1 && errno != EEXIST) {
			perror("Failed to create pipe");
			return 1;
		}
		if (messagestr) 
			pipe_fd = open(pipename, O_RDONLY | O_NONBLOCK );
		else 
			pipe_fd = open(pipename, O_RDONLY );
		if (pipe_fd == -1) {
			perror("failed to open shared pipe");
			return 1;
		}
	}
    /* Print Running Stuff */
    if ((optind < argc) && (!messagestr)) {
      int i;
      matrix_clear();
      for (i=optind;i<argc;i++)  {
      	matrix[i-optind] = strtoul(argv[i],0L,16);
      }
       matrix_render();
        if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS)
        {
            fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
        }
    }
    else {
	if (messagestr) {
		strtocmdbuf(messagestr);
	}
    /* Long running display */
    while (running)
    {
	    int i,x=0;
	    if (cmdbuf) {
		switch (anim) {
		case ANIM_FADE:
			if (animphase >  7) animphase=0;
			    for (i=0;i<led_count;i++) {
				    if (++x >= cmdbuf_len) x=0;
				    matrix[i] = 
					    	(((cmdbuf[x] & 0xff0000) >> animphase)  & 0xff0000)  | 
					    	(((cmdbuf[x] & 0xff00) >> animphase)  & 0x00ff00)  | 
					    	(((cmdbuf[x] & 0xff) >> animphase)  & 0xff);
			    }
			    animphase++;
			break;
		case ANIM_BLINK:
			if (animphase >  16) animphase=0;
			    for (i=0;i<led_count;i++) {
				    if (++x >= cmdbuf_len) x=0;
				    int p = animphase;
				    if (p>=8) p= 16-p;
				    matrix[i] = 
					    	(((cmdbuf[x] & 0xff0000) >> p)  & 0xff0000)  | 
					    	(((cmdbuf[x] & 0xff00) >> p)  & 0x00ff00)  | 
					    	(((cmdbuf[x] & 0xff) >> p)  & 0xff);
			    }
			    animphase++;
			break;
		case ANIM_WALK:
			    for (i=0;i<led_count;i++) {
				    if (++x >= cmdbuf_len) x=0;
				    int p = (animphase+i)%16;
					if (p >  16) p=0;
				    if (p>=8) p= 16-p;
				    matrix[i] = 
					    	(((cmdbuf[x] & 0xff0000) >> p)  & 0xff0000)  | 
					    	(((cmdbuf[x] & 0xff00) >> p)  & 0x00ff00)  | 
					    	(((cmdbuf[x] & 0xff) >> p)  & 0xff);
			    }
			    animphase++;
			break;
		case ANIM_WINK:
			if (animphase >  led_count) animphase=0;
			    for (i=0;i<led_count;i++) {
				    if (++x >= cmdbuf_len) x=0;
				    if (i == animphase) {
				    matrix[i] = 0;
				    } else 
				    matrix[i] = cmdbuf[x];
			    }
			    animphase++;
			break;
		case ANIM_SIN:
			        for (i = 0; i < led_count; i++) {
			    //int cmd_idx = (i + (int)animphase) % cmdbuf_len;
			    int cmd_idx = i % cmdbuf_len;
			    unsigned int original_color = cmdbuf[cmd_idx];

			    float normalized_position = (float)i / (led_count - 1);
			    if (led_count == 1) normalized_position = 0.0f;

			    //float phase = (normalized_position * SINE_WAVE_FREQUENCY * 2.0f * M_PI) + ((float)animphase * 0.1f * 2.0f * M_PI / led_count);
			    float phase = (normalized_position * FREQ_FACTOR) + ((float)animphase * ANIM_PHASE_SCALE / led_count);

			    float sine_value = sinf(phase); // * sinf(phase);

			    float intensity_multiplier = SINE_WAVE_OFFSET + (sine_value * SINE_WAVE_AMPLITUDE);

			    if (intensity_multiplier < 0.0f) intensity_multiplier = 0.0f;
			    if (intensity_multiplier > 1.0f) intensity_multiplier = 1.0f;

			    unsigned int red = (original_color >> 16) & 0xFF;
			    unsigned int green = (original_color >> 8) & 0xFF;
			    unsigned int blue = original_color & 0xFF;

			    //intensity_multiplier = 1- intensity_multiplier;
			    red = (unsigned int)(red * intensity_multiplier);
			    green = (unsigned int)(green * intensity_multiplier);
			    blue = (unsigned int)(blue * intensity_multiplier);

			    matrix[i] = (red << 16) | (green << 8) | blue;
			}
			animphase++;
			break;
		  default:
		    for (i=0;i<led_count;i++) {
			    if (++x >= cmdbuf_len) x=0;
			    matrix[i] = cmdbuf[x];
		    }
		    break;
		}
	    }
        matrix_render();

        if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS)
        {
            fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
            break;
        }

        // 15 frames /sec
        usleep(udelay );

	if (pipe_fd != -1) {
		char new_message[1024];
		int bytes_read =
		    read(pipe_fd, new_message, sizeof(new_message));
		if (bytes_read == 0) {
			
			if (anim == ANIM_NORMAL) {
				close(pipe_fd);
				pipe_fd = open(pipename, O_RDONLY);
				if (pipe_fd == -1) {
					perror("failed to reopen shared pipe");
					return 1;
				}
				}
			
			}
		else if (bytes_read >= 1) {
			new_message[bytes_read]=(char) 0;
			strtocmdbuf(new_message);
			animphase=0;
		}
	}
    }
	}
    if (clear_on_exit) {
	matrix_clear();
	matrix_render();
	ws2811_render(&ledstring);
    }

    ws2811_fini(&ledstring);

    printf ("\n");
    return ret;
}
