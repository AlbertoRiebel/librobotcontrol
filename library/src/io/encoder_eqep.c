/**
 * @file encoder_eqep.c
 *
 * @author     James Strawson
 * @date       1/29/2018
 *
 * --- Port to kernel 5.10 (BeagleBone Blue, Debian 11) ---
 *
 * rc_encoder_eqep_init() now also configures the 'ceiling' attribute
 * of each channel. The kernel 5.10 ti-eqep driver (Linux 'counter'
 * subsystem ABI) leaves 'ceiling' at 0 by default, unlike whatever
 * the kernel 4.14 driver did internally. With ceiling=0 the hardware
 * position register never accumulates past a single step, so every
 * channel would silently read back 0 forever without this fix -
 * confirmed on real hardware across all three channels (eqep0/1/2)
 * during the 5.10 port.
 *
 * The per-channel setup was factored into init_channel() so the
 * disable -> ceiling -> zero -> enable sequence lives in exactly one
 * place instead of being tripled (which is how the original file was
 * structured, and how a copy-paste bug had crept into channel 2's
 * comment in the upstream source).
 */

#include <stdio.h>
#include <stdlib.h> // for atoi
#include <string.h> // for strlen
#include <errno.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close, write

#include <rc/model.h>
#include <rc/encoder_eqep.h>

// preposessor macros
#define unlikely(x)     __builtin_expect (!!(x), 0)

#define MAX_BUF 64

#define EQEP_BASE0 "/sys/bus/platform/devices/48300000.epwmss/48300180.counter/counter0"
#define EQEP_BASE1 "/sys/bus/platform/devices/48302000.epwmss/48302180.counter/counter1"
#define EQEP_BASE2 "/sys/bus/platform/devices/48304000.epwmss/48304180.counter/counter2"

/*
 * Max value for the 'ceiling' attribute (32-bit unsigned register on
 * the AM335x eQEP hardware). Setting it this high means the counter
 * will not wrap around during any realistic amount of encoder use,
 * matching how the old kernel 4.14 driver behaved (no user-visible
 * ceiling concept at all - the raw 32-bit register just counted).
 */
#define EQEP_CEILING_STR "4294967295"

static int fd[3]; //store file descriptors for 3 position files
static int init_flag = 0; // boolean to check if mem mapped


/**
 * Opens a single sysfs attribute file, writes one string to it, and
 * closes it again. Used for the one-shot writes (enable, ceiling)
 * that don't need to stay open, unlike the 'count' fd which is kept
 * open for the lifetime of the driver so rc_encoder_eqep_read/write
 * can lseek+read/write it repeatedly without re-opening each time.
 *
 * Returns 0 on success, -1 on failure (message already printed).
 */
static int write_sysfs_attr(const char* path, const char* value)
{
        int attr_fd = open(path, O_WRONLY);
        if(unlikely(attr_fd<0)){
                fprintf(stderr, "ERROR in rc_encoder_eqep_init, failed to open %s\n", path);
                perror("  reason");
                fprintf(stderr, "Perhaps kernel or device tree is too old\n");
                return -1;
        }
        if(unlikely(write(attr_fd, value, strlen(value))==-1)){
                fprintf(stderr, "ERROR in rc_encoder_eqep_init, failed to write '%s' to %s\n", value, path);
                perror("  reason");
                close(attr_fd);
                return -1;
        }
        close(attr_fd);
        return 0;
}


/**
 * Configures one eQEP channel: disable, set ceiling to max, zero the
 * position, then re-enable. Returns an open fd to that channel's
 * 'count' attribute (left open for the caller to store in fd[]), or
 * -1 on any error.
 *
 * base_path is one of EQEP_BASE0/1/2.
 */
static int init_channel(const char* base_path)
{
        char path[256];
        int count_fd;

        // disable first so ceiling/count changes apply to a channel
        // that isn't actively latching hardware transitions mid-write
        snprintf(path, sizeof(path), "%s/count0/enable", base_path);
        if(unlikely(write_sysfs_attr(path, "0")==-1)) return -1;

        // this is the actual fix: without raising ceiling above 0,
        // the position never accumulates past a single step
        snprintf(path, sizeof(path), "%s/count0/ceiling", base_path);
        if(unlikely(write_sysfs_attr(path, EQEP_CEILING_STR)==-1)) return -1;

        snprintf(path, sizeof(path), "%s/count0/count", base_path);
        count_fd = open(path, O_RDWR);
        if(unlikely(count_fd<0)){
                fprintf(stderr, "ERROR in rc_encoder_eqep_init, failed to open %s\n", path);
                perror("  reason");
                fprintf(stderr, "Perhaps kernel or device tree is too old\n");
                return -1;
        }
        if(unlikely(write(count_fd,"0",2)==-1)){
                perror("ERROR in rc_encoder_eqep_init, failed to zero out position");
                close(count_fd);
                return -1;
        }

        snprintf(path, sizeof(path), "%s/count0/enable", base_path);
        if(unlikely(write_sysfs_attr(path, "1")==-1)){
                close(count_fd);
                return -1;
        }

        return count_fd;
}


int rc_encoder_eqep_init(void)
{
        if(init_flag) return 0;

        fd[0] = init_channel(EQEP_BASE0);
        if(unlikely(fd[0]<0)) return -1;

        // channel 2 (eqep1) - skipped on the Pocket, which lacks this unit
        if(rc_model()!=MODEL_BB_POCKET){
                fd[1] = init_channel(EQEP_BASE1);
                if(unlikely(fd[1]<0)) return -1;
        }

        fd[2] = init_channel(EQEP_BASE2);
        if(unlikely(fd[2]<0)) return -1;

        init_flag = 1;
        return 0;
}

int rc_encoder_eqep_cleanup(void)
{
        int i;
        for(i=0;i<3;i++){
                close(fd[i]);
        }
        init_flag = 0;
        return 0;
}


int rc_encoder_eqep_read(int ch)
{
        char buf[12];

        //sanity checks
        if(unlikely(!init_flag)){
                fprintf(stderr,"ERROR in rc_encoder_eqep_read, please initialize with rc_encoder_eqep_init() first\n");
                return -1;
        }
        if(unlikely(ch==4)){
                fprintf(stderr,"ERROR in rc_encoder_eqep_read, channel 4 is read by the PRU, use rc_encoder_pru_read instead\n");
                return -1;
        }
        if(unlikely(ch<1 || ch>3)){
                fprintf(stderr,"ERROR: in rc_encoder_eqep_read, encoder channel must be between 1 & 3 inclusive\n");
                return -1;
        }
        // seek to beginning of file and read
        if(unlikely(lseek(fd[ch-1],0,SEEK_SET)<0)){
                perror("ERROR: in rc_encoder_eqep_read, failed to seek to beginning of fd");
                return -1;
        }
        if(unlikely(read(fd[ch-1], buf, sizeof(buf))==-1)){
                perror("ERROR in rc_encoder_eqep_read, can't read position fd");
                return -1;
        }
        return atoi(buf);
}



int rc_encoder_eqep_write(int ch, int pos)
{
        char buf[12];
        //sanity checks
        if(unlikely(!init_flag)){
                fprintf(stderr,"ERROR in rc_encoder_eqep_write, please initialize with rc_encoder_eqep_init() first\n");
                return -1;
        }
        if(unlikely(ch==4)){
                fprintf(stderr,"ERROR in rc_encoder_eqep_write, channel 4 is read by the PRU, use rc_encoder_pru_write instead\n");
                return -1;
        }
        if(unlikely(ch<1 || ch>3)){
                fprintf(stderr,"ERROR: in rc_encoder_eqep_write, encoder channel must be between 1 & 3 inclusive\n");
                return -1;
        }
        if(unlikely(lseek(fd[ch-1],0,SEEK_SET)<0)){
                perror("ERROR: in rc_encoder_eqep_write, failed to seek to beginning of fd");
                return -1;
        }
        sprintf(buf,"%d",pos);
        if(unlikely(write(fd[ch-1], buf, sizeof(buf))==-1)){
                perror("ERROR in rc_encoder_eqep_write, can't write position fd");
                return -1;
        }
        return 0;
}
