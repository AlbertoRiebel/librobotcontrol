/**
 * @file pru.c
 * @brief PRU management stub for kernel 5.10
 *
 * PRU lifecycle is managed by systemd/remoteproc externally.
 * Shared memory access via /dev/mem is blocked by CONFIG_STRICT_DEVMEM.
 * Communication is handled via RPMsg in servo.c and encoder_pru.c.
 */

#include <stdio.h>
#include <stdint.h>
#include <rc/pru.h>

int rc_pru_start(int ch, const char* fw_name)
{
        return 0;
}

int rc_pru_stop(int ch)
{
        return 0;
}

volatile uint32_t* rc_pru_shared_mem_ptr(void)
{
        return NULL;
}
