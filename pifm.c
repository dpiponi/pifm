//
// Modified from code at https://github.com/Emerica/pifm by Dan Piponi
//

//
// Page numbers in comments refer to
// http://www.raspberrypi.org/wp-content/uploads/2012/02/BCM2835-ARM-Peripherals.pdf
//

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <unistd.h>

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

volatile unsigned char *allof7e;

#define ACCESS(offset, type) (*(volatile type*)(offset+(int)allof7e-0x7e000000))

#define CM_GP0CTL (0x7e101070) // p.107
#define GPFSEL0 (0x7E200000)   // p.90
#define CM_GP0DIV (0x7e101074) // p.108

void setup_fm(int state) {
    int mem_fd = open("/dev/mem", O_RDWR|O_SYNC);
    if (mem_fd < 0) {
        printf("can't open /dev/mem\n");
        exit(-1);
    }
    allof7e = (unsigned char *)mmap(
                  NULL,
                  0x01000000,  // len
                  PROT_READ|PROT_WRITE,
                  MAP_SHARED,
                  mem_fd,
                  0x20000000  // See http://www.raspberrypi.org/forums/viewtopic.php?t=8476
              );

    if (allof7e == (unsigned char *)-1) {
        exit(-1);
    }

    //
    // Set up GPIO 4 to pulse regularly at a given period.
    //
    struct GPFSEL0_T {
        char FSEL0 : 3;
        char FSEL1 : 3;
        char FSEL2 : 3;
        char FSEL3 : 3;
        char FSEL4 : 3;
        char FSEL5 : 3;
        char FSEL6 : 3;
        char FSEL7 : 3;
        char FSEL8 : 3;
        char FSEL9 : 3;
        char RESERVED : 2;
    };
    ACCESS(GPFSEL0, struct GPFSEL0_T).FSEL4 = 4; // Alternative function 0 (see p.92)

    //
    // The 6 means select PLLD
    // I guess this is the 500MHz PLL.
    // See p.107
    //
    struct GPCTL {
        char SRC         : 4;
        char ENAB        : 1;
        char KILL        : 1;
        char             : 1;
        char BUSY        : 1;
        char FLIP        : 1;
        char MASH        : 2;
        unsigned int     : 13;
        char PASSWD      : 8;
    };
    ACCESS(CM_GP0CTL, struct GPCTL) = (struct GPCTL) {6, state, 0, 0, 0, state, 0x5a };
}

void shutdown_fm() {
    static int shutdown = 0;
    if (!shutdown) {
        shutdown = 1;
        printf("\nShutting Down\n");
        setup_fm(0);
        exit(0);
    }
}

//
// Set square wave period. See p. 105 and 108
// Although DIV is 24 bit the period can only be set to
// an accuracy of 12 bits.
// The first 12 bits control the pulse length in units of 1/500MHz.
// The next 12 bits are used to
// dither the period so it averages at the chosen 24 bit value.
// The resulting quare wave is then filtered using MASH
// See p.105 and http://en.wikipedia.org/wiki/MASH_(modulator)#Decimation_structures
//
// The 0x5a is a "password"
//
void modulate(int period) {
    struct CM_GP0DIV_T {
        unsigned int DIV : 24;
        char PASSWD : 8;
    };

    ACCESS(CM_GP0DIV, struct CM_GP0DIV_T) = (struct CM_GP0DIV_T) { period, 0x5a };
}

void delay(int n) {
    volatile static int clock = 0;
    for (int i = 0; i < n; ++i) {
        ++clock;
    }
}

void playWav(char *filename, int mod, float bw) {
    int fp = open(filename, 'r');
    lseek(fp, 22, SEEK_SET); //Skip header
    short *data = (short *)malloc(1024);
    printf("Now broadcasting: %s\n", filename);

    while (read(fp, data, 1024)) {

        for (int j = 0; j<1024/2; j++) {
            //
            // Compute modulated carrier period
            //
            float dval = (int)floor((float)(data[j])/65536.0f*bw);

            //
            // Disassembling delay shows a 6 instruction loop
            // I'm guessing 1 clock cycle per instruction.
            // Clock is 850MHz under load.
            // Assuming audio at 22.050 KHz
            // So delay by 850000000/22050/6=6424.8
            // Subtract a little bit because of the overhead of the rest of this loop.
            //
            modulate(dval+mod);
            delay(6350);
        }
    }
}

int main(int argc, char **argv) {
    signal(SIGTERM, &shutdown_fm);
    signal(SIGINT, &shutdown_fm);
    atexit(&shutdown_fm);

    setup_fm(1);
    float freq = atof(argv[2]);
    float bw;
    int mod = (500/freq)*4096;
    modulate(mod);

    if (argc==3) {
         bw = 25.0;
         printf("Setting up modulation: %f Mhz / %d @ %f\n",freq,mod,bw);
         playWav(argv[1], mod, bw);
    } else if (argc==4) {
         bw = atof(argv[3]);
         printf("Setting up modulation: %f Mhz / %d @ %f\n",freq,mod,bw);
         playWav(argv[1], mod, bw);
    } else {
         fprintf(stderr,
                "Usage: %s wavfile.wav freq [power]\n\n"
                "Where wavfile is 16 bit 22.050kHz Mono\n"
                "Power will default to 25 if not specified. It should only be lowered!",argv[0]);
    }
    return 0;
}
