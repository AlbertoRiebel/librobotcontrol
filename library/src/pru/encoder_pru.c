/**
 * @file encoder_pru.c
 * @brief PRU encoder reading via RPMsg for kernel 5.10
 *
 * Replaces shared memory access with RPMsg communication
 * through /dev/rpmsg_pru30.
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <rc/time.h>
#include <rc/encoder_pru.h>

#define RPMSG_PRU0  "/dev/rpmsg_pru30"

static int pru_fd    = -1;
static int init_flag = 0;

int rc_encoder_pru_init(void)
{
        pru_fd = open(RPMSG_PRU0, O_RDWR);
        if(pru_fd < 0){
                fprintf(stderr, "ERROR: No se pudo abrir %s\n", RPMSG_PRU0);
                init_flag = 0;
                return -1;
        }
        init_flag = 1;
        return 0;
}

void rc_encoder_pru_cleanup(void)
{
        if(pru_fd >= 0) close(pru_fd);
        init_flag = 0;
}

int rc_encoder_pru_read(void)
{
        if(pru_fd < 0 || init_flag == 0) return -1;

        int32_t ticks = 0;
        if(write(pru_fd, "r", 1) < 0) return -1;
        if(read(pru_fd, &ticks, sizeof(int32_t)) == sizeof(int32_t))
                return (int)ticks;
        return -1;
}

int rc_encoder_pru_write(int pos)
{
        if(pru_fd < 0 || init_flag == 0) return -1;
        if(write(pru_fd, "w", 1) < 0) return -1;
        return 0;
}
