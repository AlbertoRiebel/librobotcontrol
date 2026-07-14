/**
 * @file encoder_pru.c
 * @brief PRU encoder reading via RPMsg for kernel 5.10
 *
 * Replaces shared memory access with RPMsg communication
 * through /dev/rpmsg_pru30.
 *
 * Protocol (matches pru_firmware/src/main_pru0.c):
 *   - Read:  write a single byte 'r', then read back a 4-byte int32
 *            with the current tick count.
 *   - Write: write 5 bytes: 'w' followed by the new int32_t count
 *            value (little-endian). No response is sent back.
 *            Previously this only sent the single 'w' byte with no
 *            actual value, so the PRU had no way to know what to
 *            reset the count to - fixed as part of the kernel 5.10
 *            port so rc_encoder_pru_write() actually works, matching
 *            rc_encoder_eqep_write()'s behavior for the other 3
 *            channels.
 *
 * rc_encoder_pru_init() now also resets the count to 0 automatically
 * (via rc_encoder_pru_write(0)) every time it's called, matching
 * rc_encoder_eqep_init()'s behavior for channels 1-3. Without this,
 * channel 4 kept accumulating continuously from PRU boot regardless
 * of how many times a calling program started/stopped - confirmed by
 * testing on real hardware.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h> // for memcpy
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

        /* Reset the PRU's internal tick count to 0 on every init(),
         * matching rc_encoder_eqep_init()'s behavior for channels
         * 1-3 (which explicitly zero their hardware counter each
         * time they're initialized). Without this, channel 4's
         * count keeps accumulating continuously from PRU boot
         * regardless of how many times a program starts/stops,
         * unlike the other three channels - confirmed by testing on
         * real hardware during the kernel 5.10 port. */
        if(rc_encoder_pru_write(0) < 0){
                fprintf(stderr, "WARNING: rc_encoder_pru_init, no se pudo reiniciar el contador del PRU a 0\n");
                /* not fatal - the RPMsg link itself is open and
                 * usable, it just starts from whatever count the
                 * PRU already had accumulated */
        }

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
        uint8_t msg[5];
        int32_t val = (int32_t)pos;
        msg[0] = 'w';
        memcpy(&msg[1], &val, sizeof(int32_t));
        if(write(pru_fd, msg, sizeof(msg)) < 0) return -1;
        return 0;
}
