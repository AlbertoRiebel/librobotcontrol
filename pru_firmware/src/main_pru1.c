#include <stdint.h>
#include <pru_cfg.h>
#include <pru_ctrl.h>
#include <pru_intc.h>
#include <rsc_types.h>
#include <pru_rpmsg.h>
#include "resource_table.h"
#include "intc_map_1.h"

volatile register uint32_t __R30;
volatile register uint32_t __R31;

#define HOST_INT        ((uint32_t) 1 << 31)
#define TO_ARM_HOST     18
#define FROM_ARM_HOST   19
#define CHAN_NAME       "rpmsg-pru"
#define CHAN_DESC       "Channel 31"
#define CHAN_PORT       31
#define VIRTIO_CONFIG_S_DRIVER_OK 4
#define PERIOD_CYCLES   4000000U

void main(void) {
    struct pru_rpmsg_transport transport;
    uint16_t src, dst, len;
    volatile uint8_t *status;
    uint8_t payload[8];
    int i;
    uint32_t ch_loops[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t ch;
    uint32_t loops;
    uint32_t out_mask;
    uint32_t last_cycle;
    uint32_t now;
    uint32_t elapsed = 0;

    CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;

    PRU1_CTRL.CTRL_bit.CTR_EN = 1;
    last_cycle = PRU1_CTRL.CYCLE;

    CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;
    status = &resourceTable.rpmsg_vdev.status;
    while (!(*status & VIRTIO_CONFIG_S_DRIVER_OK));
    pru_rpmsg_init(&transport,
                   &resourceTable.rpmsg_vring0,
                   &resourceTable.rpmsg_vring1,
                   TO_ARM_HOST, FROM_ARM_HOST);
    while (pru_rpmsg_channel(RPMSG_NS_CREATE, &transport,
                         CHAN_NAME, CHAN_DESC, CHAN_PORT) != PRU_RPMSG_SUCCESS);

    uint32_t loop_count = 0;
    uint32_t loops_per_period = 93712; /* ~20ms a ~3MHz efectivos del loop */

    while (1) {
        loop_count++;
        if (loop_count >= loops_per_period) loop_count = 0;

        /* Revisar mensajes solo cada N iteraciones para no saturar */
        if ((loop_count & 0xFF) == 0) {
            if (__R31 & HOST_INT) {
                CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;
                if (pru_rpmsg_receive(&transport, &src, &dst,
                                      payload, &len) == PRU_RPMSG_SUCCESS) {
                    if (len == 8) {
                        ch    = ((uint32_t*)payload)[0];
                        loops = ((uint32_t*)payload)[1];
                        if (ch >= 1 && ch <= 8) {
                            ch_loops[ch - 1] = loops;
                        } else if (ch == 0) {
                            for (i = 0; i < 8; i++) ch_loops[i] = loops;
                        }
                    }
                }
            }
        }

        out_mask = 0;
        if (loop_count < ch_loops[0]) out_mask |= (1 << 8);
        if (loop_count < ch_loops[1]) out_mask |= (1 << 10);
        if (loop_count < ch_loops[2]) out_mask |= (1 << 9);
        if (loop_count < ch_loops[3]) out_mask |= (1 << 11);
        if (loop_count < ch_loops[4]) out_mask |= (1 << 6);
        if (loop_count < ch_loops[5]) out_mask |= (1 << 7);
        if (loop_count < ch_loops[6]) out_mask |= (1 << 4);
        if (loop_count < ch_loops[7]) out_mask |= (1 << 5);

        __R30 = out_mask;
    }
}
