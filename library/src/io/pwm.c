/*
 * @file pwm.c
 *
 * C pwm interface for Beaglebone boards
*/

#include <stdio.h>
#include <stdlib.h> // for atoi
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <glob.h>
#include <rc/pwm.h>
#include <rc/time.h>

#define MIN_HZ 1
#define MAX_HZ 1000000000
#define MAXBUF 128
#define SYS_DIR "/sys/class/pwm"

// ocp only used for exporting right now, everything else through SYS_DIR
// to allow for shorter strings and neater code.
#define OCP_DIR "/sys/devices/platform/ocp/4830%d000.epwmss/4830%d200.pwm/pwm"
#define OCP_OFFSET      66

// preposessor macros
#define unlikely(x)     __builtin_expect (!!(x), 0)

// variables
static int dutyA_fd[3];                 // pointers to duty cycle file descriptor
static int dutyB_fd[3];                 // pointers to duty cycle file descriptor
static unsigned int period_ns[3];       // one period per subsystem
static int init_flag[3] = {0,0,0};

// The ti pwm driver has gone through several revisions and it presents devices
// in the file system differently every version. For example, subsytem 2 channel A
// showed up as the following files:
// in 4.9:            /sys/class/pwm/pwmchip4/pwm0/
// in 4.14.54-ti-r63: /sys/class/pwm/pwm-4:0/
// in 4.14.61-ti-r68: /sys/class/pwm/pwm-7:0/
// depending on kernel, mode is set to 0 or 1 on export, index is used for 4.14
static int mode; // 0 for "pwmx", 1 for "pwm-x:y" versions of driver
static int ssindex[3]; // index given by the kernel to each pwm chip when in mode 1

/*
 * @brief      exports A and B pwm channels
 *
 * @param[in]  ss    subsystem, to export
 *
 * @return     0 on succcess, -1 on failure
*/

// --- KERNEL 5.10 PATCH START ---

// Chip matrix mapping
static int get_chip(int ss) {
        if(ss==0) return 3; // SS0 eHRPWM (Handles Channels A and B)
        if(ss==1) return 5; // SS1 eHRPWM (Handles Channels A and B - Motors 3 and 4)
        if(ss==2) return 7; // SS2 eHRPWM (Handles Channels A and B - Motors 1 and 2)
        return -1;
}

static int __export_channels(int ss) {
        int export_fd;
        char buf[MAXBUF];
        int chip = get_chip(ss);

        snprintf(buf, sizeof(buf), SYS_DIR "/pwmchip%d/export", chip);
        export_fd = open(buf, O_WRONLY);
        if(export_fd >= 0){
                write(export_fd, "0", 2); // Export Channel A (pwm0)
                write(export_fd, "1", 2); // Export Channel B (pwm1)
                close(export_fd);
        }
        return 0;
}

static int __unexport_channels(int ss) {
        int unexport_fd;
        char buf[MAXBUF];
        int chip = get_chip(ss);

        snprintf(buf, sizeof(buf), SYS_DIR "/pwmchip%d/unexport", chip);
        unexport_fd = open(buf, O_WRONLY);
        if(unexport_fd >= 0){
                write(unexport_fd, "0", 2);
                write(unexport_fd, "1", 2);
                close(unexport_fd);
        }
        return 0;
}

int rc_pwm_init(int ss, int frequency) {
        int periodA_fd, periodB_fd;
        int enableA_fd, enableB_fd;
        int polarityA_fd, polarityB_fd;
        char buf[MAXBUF];
        int len;

        if(ss<0 || ss>2 || frequency<MIN_HZ || frequency>MAX_HZ) return -1;

        __unexport_channels(ss);
        __export_channels(ss);

        rc_usleep(600000); // Give Linux time to create the virtual files

        int chip = get_chip(ss);

        // Open Duty Cycle
	snprintf(buf, sizeof(buf), SYS_DIR "/pwmchip%d/pwm0/duty_cycle", chip);
        dutyA_fd[ss] = open(buf,O_WRONLY);
        snprintf(buf, sizeof(buf), SYS_DIR "/pwmchip%d/pwm1/duty_cycle", chip);
        dutyB_fd[ss] = open(buf,O_WRONLY);
        if(dutyA_fd[ss]==-1 || dutyB_fd[ss]==-1) return -1;

	// Open Enable
	snprintf(buf, sizeof(buf), SYS_DIR "/pwmchip%d/pwm0/enable", chip);
        enableA_fd = open(buf,O_WRONLY);
        snprintf(buf, sizeof(buf), SYS_DIR "/pwmchip%d/pwm1/enable", chip);
        enableB_fd = open(buf,O_WRONLY);

        // Open Period
	snprintf(buf, sizeof(buf), SYS_DIR "/pwmchip%d/pwm0/period", chip);
        periodA_fd = open(buf,O_WRONLY);
        snprintf(buf, sizeof(buf), SYS_DIR "/pwmchip%d/pwm1/period", chip);
        periodB_fd = open(buf,O_WRONLY);

        // Open Polarity
        snprintf(buf, sizeof(buf), SYS_DIR "/pwmchip%d/pwm0/polarity", chip);
        polarityA_fd = open(buf,O_WRONLY);
        snprintf(buf, sizeof(buf), SYS_DIR "/pwmchip%d/pwm1/polarity", chip);
        polarityB_fd = open(buf,O_WRONLY);

        // Configure Period
        period_ns[ss] = 1000000000/frequency;
        len = snprintf(buf, sizeof(buf), "%d", period_ns[ss]);
        if(periodA_fd >= 0) write(periodA_fd, buf, len);
        if(periodB_fd >= 0) write(periodB_fd, buf, len);

        // Configure Normal Polarity
        if(polarityA_fd >= 0) write(polarityA_fd, "normal", 7);
        if(polarityB_fd >= 0) write(polarityB_fd, "normal", 7);

        // Ensure Motors are Off (Duty 0)
        write(dutyA_fd[ss], "0", 2);
        write(dutyB_fd[ss], "0", 2);

        // Enable Subsystem
        if(enableA_fd >= 0) write(enableA_fd, "1", 2);
        if(enableB_fd >= 0) write(enableB_fd, "1", 2);

        // Close configuration file descriptors
        if(enableA_fd >= 0) close(enableA_fd);
        if(enableB_fd >= 0) close(enableB_fd);
        if(periodA_fd >= 0) close(periodA_fd);
        if(periodB_fd >= 0) close(periodB_fd);
        if(polarityA_fd >= 0) close(polarityA_fd);
        if(polarityB_fd >= 0) close(polarityB_fd);

        init_flag[ss] = 1;
        return 0;
}

int rc_pwm_cleanup(int ss) {
        int enableA_fd, enableB_fd;
        char buf[MAXBUF];

        if(ss<0 || ss>2 || init_flag[ss]==0) return 0;

        int chip = get_chip(ss);

        snprintf(buf, sizeof(buf), SYS_DIR "/pwmchip%d/pwm0/enable", chip);
        enableA_fd = open(buf,O_WRONLY);
        snprintf(buf, sizeof(buf), SYS_DIR "/pwmchip%d/pwm1/enable", chip);
        enableB_fd = open(buf,O_WRONLY);

        write(dutyA_fd[ss], "0", 2);
        write(dutyB_fd[ss], "0", 2);

        if(enableA_fd >= 0) { write(enableA_fd, "0", 2); close(enableA_fd); }
        if(enableB_fd >= 0) { write(enableB_fd, "0", 2); close(enableB_fd); }

        close(dutyA_fd[ss]);
        close(dutyB_fd[ss]);

        __unexport_channels(ss);
        init_flag[ss] = 0;
        return 0;
}
// --- KERNEL 5.10 PATCH END ---

int rc_pwm_set_duty(int ss, char ch, double duty) {
        int len, ret, duty_ns;
        char buf[MAXBUF];

        // sanity checks
        if(unlikely(ss<0 || ss>2)){
                fprintf(stderr,"ERROR in rc_pwm_set_duty, PWM subsystem must be between 0 and 2\n");
                return -1;
        }
        if(unlikely(init_flag[ss]==0)){
                fprintf(stderr, "ERROR in rc_pwm_set_duty, subsystem %d not initialized yet\n", ss);
                return -1;
        }
        if(unlikely(duty > 1.0 || duty<0.0)){
                fprintf(stderr,"ERROR in rc_pwm_set_duty, duty must be between 0.0f & 1.0f\n");
                return -1;
        }

        // set the duty
        duty_ns = duty*period_ns[ss];
        len = snprintf(buf, sizeof(buf), "%d", duty_ns);
        switch(ch){
        case 'A':
                ret=write(dutyA_fd[ss], buf, len);
                break;
        case 'B':
                ret=write(dutyB_fd[ss], buf, len);
                break;
        default:
                fprintf(stderr,"ERROR in rc_pwm_set_duty_ns, pwm channel must be 'A' or 'B'\n");
                return -1;
        }

        // make sure write worked
        if(unlikely(ret==-1)){
                perror("ERROR in rc_pwm_set_duty_ns, failed to write to duty_cycle fd");
                return -1;
        }
        return 0;
}


int rc_pwm_set_duty_ns(int ss, char ch, unsigned int duty_ns){
        int len, ret;
        char buf[MAXBUF];

        // sanity checks
        if(unlikely(ss<0 || ss>2)){
                fprintf(stderr,"ERROR in rc_pwm_set_duty_ns, PWM subsystem must be between 0 and 2\n");
                return -1;
        }
        if(unlikely(init_flag[ss]==0)){
                fprintf(stderr, "ERROR in rc_pwm_set_duty_ns, subsystem %d not initialized yet\n", ss);
                return -1;
        }
        if(unlikely(duty_ns>period_ns[ss])){
                fprintf(stderr,"ERROR in rc_pwm_set_duty_ns, duty must be between 0 & %d for current frequency\n", period_ns[ss]);
                return -1;
        }

        // set the duty
        len = snprintf(buf, sizeof(buf), "%d", duty_ns);
        switch(ch){
        case 'A':
                ret=write(dutyA_fd[ss], buf, len);
                break;
        case 'B':
                ret=write(dutyB_fd[ss], buf, len);
                break;
        default:
                fprintf(stderr,"ERROR in rc_pwm_set_duty_ns, pwm channel must be 'A' or 'B'\n");
                return -1;
        }

        // make sure write worked
        if(unlikely(ret==-1)){
                perror("ERROR in rc_pwm_set_duty_ns, failed to write to duty_cycle fd");
                return -1;
        }
        return 0;
}
