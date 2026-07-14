/*
 * Copyright (C) 2021 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the
 *        distribution.
 *
 *      * Neither the name of Texas Instruments Incorporated nor the names of
 *        its contributors may be used to endorse or promote products derived
 *        from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * intc_map_0.h - PRU0 interrupt controller map.
 *
 * FROM_ARM_HOST = 17. This is inferred (not directly read from any
 * DTB) from the TO/FROM pairing pattern confirmed for PRU1: the
 * running system's own compiled DTB shows &pru1's "vring" interrupt
 * at sysevt 18 (matching main_pru1.c's TO_ARM_HOST=18 exactly), and
 * main_pru1.c/intc_map_1.h independently define FROM_ARM_HOST=19
 * (TO+1). The same compiled DTB shows &pru0's own vring interrupt at
 * sysevt 16 (confirmed), so by the same TO/FROM=TO+1 pairing pattern,
 * PRU0's FROM_ARM_HOST should be 17. Channel/host (1,1) mirrors
 * PRU1's own choice exactly - each PRU core has its own independent
 * host0/host1 interrupt lines (per the architectural note at the top
 * of intc_map_1.h: host interrupts 0-1 are PRU-internal only, host
 * interrupts 2-9 go to the ARM Linux host and are configured via the
 * device tree instead, which is exactly where the confirmed vring
 * sysevt 16/18 values come from).
 *
 * If this value is wrong, the PRU will simply never react to
 * messages sent from the ARM side (rc_encoder_pru_read() would hang
 * or timeout) - a visible, debuggable symptom, not silent data
 * corruption.
 */

#ifndef _INTC_MAP_0_H_
#define _INTC_MAP_0_H_

#include <stddef.h>
#include <rsc_types.h>

#pragma DATA_SECTION(my_irq_rsc, ".pru_irq_map")
#pragma RETAIN(my_irq_rsc)

struct pru_irq_rsc my_irq_rsc = {
        0,      /* type = 0 */
        1,      /* number of system events being mapped */
        {
                {17, 1, 1},     /* FROM_ARM_HOST: sysevt 17, channel 1, host 1 */
        },
};

#endif /* _INTC_MAP_0_H_ */
