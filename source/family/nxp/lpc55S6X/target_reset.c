/**
 * @file    target_reset.c
 * @brief   Target reset for the lpc812
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2009-2019, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "target_family.h"
#include "swd_host.h"
#include "cmsis_os2.h"
#include "daplink_debug.h"

#define NVIC_Addr               (0xE000E000)

#define DBG_Addr                (0xE000EDF0)
#define DWT_COMP0               (0xE0001020)
#define DWT_FUNCTION0           (0xE0001028)
#define DWT_FUNCTION_MATCH      (0x4 << 0)
#define DWT_FUNCTION_ACTION     (0x1 << 4)
#define DWT_FUNCTION_DATAVSIZE  (0x2 << 10)

/* Debug mailbox AP registers */
#define DEBUGMB_CSW  0x02000000
#define DEBUGMB_REQ  0x02000004
#define DEBUGMB_RET  0x02000008
#define DEBUGMB_ID   0x020000FC

static uint8_t lpc55s6x_target_set_state(target_state_t state)
{
    uint32_t val;
    uint32_t i;
    int8_t ap_retries = 2;

    if (state == RESET_PROGRAM) {
        if (!swd_init_debug()) {
            debug_msg("Init Fail\r\n");
            return 0;
        }

        // Enable debug
        while(swd_write_word(DBG_HCSR, DBGKEY | C_DEBUGEN) == 0) {
            if( --ap_retries <=0 ) {
                debug_msg("Failed to enable debug\r\n");
                return 0;
            }
            // Target is in invalid state?
            swd_set_target_reset(1);
            osDelay(2);
            swd_set_target_reset(0);
            osDelay(2);
        }
#if 0
        // Set Trace Enable bit
        if (!swd_read_word(DBG_EMCR, &val)) {
            return 0;
        }

        if (!swd_write_word(DBG_EMCR, val | TRCENA)) {
            return 0;
        }

        // Clear the comparator function register
        if (!swd_write_word(DWT_FUNCTION0, 0x0)) {
            return 0;
        }

        // Set the address
        if (!swd_write_word(DWT_COMP0, 0x50000040)) {
            return 0;
        }

        // Update the comparator function register
        if (!swd_write_word(DWT_FUNCTION0, (DWT_FUNCTION_MATCH | DWT_FUNCTION_ACTION | DWT_FUNCTION_DATAVSIZE))) {
            return 0;
        }

        // Reset using the debug mailbox
        if (!swd_write_ap(DEBUGMB_CSW, 0x20)) {
            return 0;
        }

        osDelay(5);
#endif

        //if (val == 0x8) {
            // Might be a newer silicon revision
            // Write Reset + Resynch request using the debug mailbox
            val = 0;
            if (!swd_read_ap(DEBUGMB_ID, &val)) {
                debug_msg("1.Read mailbox ID fail\r\n");
                return 0;
            }

            //debug_msg("1.MB ID fail=%x\r\n", val);

            if (!swd_write_ap(DEBUGMB_CSW, 0x21)) {
                debug_msg("Write reset fail\r\n");
                return 0;
            }

            osDelay(5);

            // Poll CSW register for zero return, indicating success
            val = 1;
            while (val != 0) {
                if (!swd_read_ap(DEBUGMB_CSW, &val)) {
                    debug_msg("Read DEBUGMB_CSW fail\r\n");
                    return 0;
                }
            }
            //debug_msg("CSW=%x\r\n", val);


            val = 0;
            if (!swd_read_ap(DEBUGMB_ID, &val)) {
                debug_msg("1.5.Read mailbox ID fail\r\n");
                return 0;
            }

            //debug_msg("1.5.MB ID=%x\r\n", val);

            // Write START_DEBUG_SESSION command to request register
            if (!swd_write_ap(DEBUGMB_REQ, 0x7)) {
                debug_msg("Write 7 fail\r\n");
                return 0;
            }

            osDelay(5);

            // Poll return register for zero return
            val = 1;
            while ((val & 0xFFFF) != 0) {
                if (!swd_read_ap(DEBUGMB_RET, &val)) {
                    debug_msg("DEBUGMB_RET fail\r\n");
                    return 0;
                }
            }
            //debug_msg("RET=%x\r\n", val);

            osDelay(100);
        //}

        //do an abort on stale target
        swd_write_dp(DP_ABORT, DAPABORT);

        // Enable debug and halt the core (DHCSR <- 0xA05F0003)
        if (!swd_write_word(DBG_HCSR, DBGKEY | C_DEBUGEN | C_HALT)) {
            debug_msg("DHCSR write FAIL\r\n");
            return 0;
        }


        ap_retries = 100000;

        do {
            if (!swd_read_word(DBG_HCSR, &val)) {
                debug_msg("DHCSR FAIL\r\n");
                return 0;
            }
            if( --ap_retries <=0 ) {
                debug_msg("Failed to halt, val=%x\r\n", val);
                if (val & S_HALT) {
                    debug_msg("S_HALT set\r\n");
                    //return 1;
                }
                return 0;
            }
        } while ((val & S_HALT) == 0);

        // Update the comparator function register
        //swd_write_word(DWT_FUNCTION0, 0);

        return 1;

    } else {
        return swd_set_target_state_sw(state);
    }
}

const target_family_descriptor_t g_target_family_lpc55S6X = {
    .family_id = VENDOR_TO_FAMILY(kNXP_VendorID, 0), //ID not maching the predefined family ids
    .target_set_state = lpc55s6x_target_set_state,
};

const target_family_descriptor_t *g_target_family = &g_target_family_lpc55S6X;
