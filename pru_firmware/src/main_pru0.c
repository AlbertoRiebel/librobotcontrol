/*
 * main_pru0.c - Quadrature encoder reader for encoder channel 4 (PRU0).
 *
 * Port of the original pru0-encoder.asm (hand-written PRU assembly,
 * (c) 2015 James Strawson, converted to clpru by Mark A. Yoder) to C
 * with RPMsg instead of shared memory - shared memory access via
 * /dev/mem is blocked by CONFIG_STRICT_DEVMEM on kernel 5.10, exactly
 * the same constraint solved for the PRU1 servo firmware. This file
 * mirrors main_pru1.c's overall structure for consistency.
 *
 * Encoder signal wiring (confirmed against am335x-bone-common-univ.dtsi
 * and dt-bindings/pinctrl/am33xx.h during the kernel 5.10 port - see
 * project README):
 *   Channel A = R31 bit 14 = P8_16 (GPMC_AD14, pinctrl offset 0x038)
 *   Channel B = R31 bit 15 = P8_15 (GPMC_AD15, pinctrl offset 0x03c)
 * Both pins are configured in MUX_MODE6 (pruin) by pru_encoder_pins
 * in the board DTS.
 *
 * Decode algorithm: XOR-based single-edge detection, direction
 * determined by the OTHER channel's level at the moment of the edge.
 * This is a direct, verified-equivalent translation of the original
 * assembly's state machine - checked instruction-by-instruction
 * against a 240-transition synthetic sequence (forward + reverse)
 * plus the simultaneous-edge edge case, before being written here.
 * Simultaneous A+B transitions in a single sample are treated as
 * ambiguous and not counted, matching the original assembly exactly.
 *
 * RPMsg protocol: userspace (rc_encoder_pru_read()/rc_encoder_pru_write()
 * in encoder_pru.c) communicates via /dev/rpmsg_pru30 with two message
 * shapes, distinguished by length:
 *   - 1 byte  ('r'): read request. Firmware responds with a 4-byte
 *     int32 containing the current tick count.
 *   - 5 bytes ('w' + int32_t, little-endian): write/reset request.
 *     Firmware sets its internal count to the given value. No
 *     response is sent back for a write.
 */

#include <stdint.h>
#include <string.h>
#include <pru_cfg.h>
#include <pru_ctrl.h>
#include <pru_intc.h>
#include <rsc_types.h>
#include <pru_rpmsg.h>
#include "resource_table.h"
#include "intc_map_0.h"

volatile register uint32_t __R30;
volatile register uint32_t __R31;

#define HOST_INT        ((uint32_t) 1 << 31)
#define TO_ARM_HOST     16   /* confirmed: matches &pru0's DTB vring interrupt (sysevt 16) */
#define FROM_ARM_HOST   17   /* inferred by TO/FROM pairing pattern - see intc_map_0.h comment */
#define CHAN_NAME       "rpmsg-pru"
#define CHAN_DESC       "Channel 30"
#define CHAN_PORT       30
#define VIRTIO_CONFIG_S_DRIVER_OK 4

/* Encoder channel A/B bit positions in R31 */
#define BIT_A ((uint32_t)1 << 14)
#define BIT_B ((uint32_t)1 << 15)

/* Check for an incoming RPMsg request every this many main-loop
 * iterations, so we don't spend PRU cycles polling on every single
 * quadrature sample. Same pattern main_pru1.c uses for its own
 * periodic message check (there: `(loop_count & 0xFF) == 0`). */
#define RPMSG_POLL_MASK 0xFFu

void main(void) {
    struct pru_rpmsg_transport transport;
    uint16_t src, dst, len;
    volatile uint8_t *status;
    uint8_t rx_payload[16];
    int32_t count = 0;
    uint32_t old_r31, new_r31, exor;
    uint32_t poll_counter = 0;

    CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;

    CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;
    status = &resourceTable.rpmsg_vdev.status;
    while (!(*status & VIRTIO_CONFIG_S_DRIVER_OK));
    pru_rpmsg_init(&transport,
                   &resourceTable.rpmsg_vring0,
                   &resourceTable.rpmsg_vring1,
                   TO_ARM_HOST, FROM_ARM_HOST);
    while (pru_rpmsg_channel(RPMSG_NS_CREATE, &transport,
                             CHAN_NAME, CHAN_DESC, CHAN_PORT) != PRU_RPMSG_SUCCESS);

    /* Seed old_r31 with whatever the pins currently read, so we don't
     * register a spurious transition on the very first sample. */
    old_r31 = __R31;

    while (1) {
        new_r31 = __R31;
        exor = old_r31 ^ new_r31;

        if ((exor & BIT_A) && !(exor & BIT_B)) {
            /* only channel A transitioned - valid single edge */
            if (new_r31 & BIT_A) {
                count += (new_r31 & BIT_B) ? -1 : 1;   /* A rose */
            } else {
                count += (new_r31 & BIT_B) ? 1 : -1;   /* A fell */
            }
        } else if ((exor & BIT_B) && !(exor & BIT_A)) {
            /* only channel B transitioned - valid single edge */
            if (new_r31 & BIT_B) {
                count += (new_r31 & BIT_A) ? 1 : -1;   /* B rose */
            } else {
                count += (new_r31 & BIT_A) ? -1 : 1;   /* B fell */
            }
        }
        /* else: nothing changed, or A and B changed on the same
         * sample (ambiguous) - don't count either way, but old_r31
         * is still resynced below regardless, matching the original
         * assembly's behavior in the simultaneous-change case. */

        old_r31 = new_r31;

        poll_counter++;
        if ((poll_counter & RPMSG_POLL_MASK) == 0) {
            if (__R31 & HOST_INT) {
                CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;
                if (pru_rpmsg_receive(&transport, &src, &dst,
                                      rx_payload, &len) == PRU_RPMSG_SUCCESS) {
                    if (len == 5 && rx_payload[0] == 'w') {
                        /* write request: bytes 1-4 are the new
                         * int32_t count value, little-endian (both
                         * PRU and ARM Cortex-A8 are little-endian,
                         * so no byte-order conversion is needed).
                         * No response is sent back for a write. */
                        memcpy(&count, &rx_payload[1], sizeof(count));
                    } else {
                        /* anything else (in practice, the 1-byte 'r'
                         * read request) is treated as a read -
                         * respond with the current count. */
                        pru_rpmsg_send(&transport, dst, src,
                                        (uint8_t*)&count, sizeof(count));
                    }
                }
            }
        }
    }
}
