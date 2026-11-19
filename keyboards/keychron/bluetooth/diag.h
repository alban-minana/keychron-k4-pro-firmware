/* Copyright 2026 Alban Minana
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Diagnostics over raw HID (USB cable), used by k4pro/tools/k4tool.py.
 *
 * Request:  [0] = DIAG_RAW_HID_ID, [1] = command
 * Response: same buffer, [2] = status (0 = OK), payload from [3], little endian.
 *
 *   0x01 INFO      protocol version, transport, Bluetooth state, battery, uptime,
 *                  scan rate, build settings, time since last key
 *   0x02 COUNTERS  7 x uint32 (see diag_counters_t)
 *   0x03 RESET     clear the counters
 *   0x04 PROBE     tap F24 through the active transport (latency measurement)
 *   0x05 OPTION    read or set a run-time option
 *   0x06 PROFILE   longest main-loop pass and longest run of each section, in us
 */

#pragma once

#include <stdint.h>

#ifndef DIAG_RAW_HID_ID
#    define DIAG_RAW_HID_ID 0xC4
#endif

typedef struct {
    uint32_t bt_reports;     // HID reports handed to the Bluetooth module
    uint32_t bt_retries;     // reports sent again after a "FIFO full" answer
    uint32_t bt_fifo_full;   // "FIFO full" answers from the module
    uint32_t bt_connects;    // entries into the connected state
    uint32_t bt_disconnects; // entries into the disconnected state
    uint32_t wake_keys;      // key reports kept while (re)connecting
    uint32_t sleeps;         // MCU low-power entries
} diag_counters_t;

extern diag_counters_t diag;

#define DIAG_INC(field) (diag.field++)

/* Section profiler (cycle counter), enabled with -DDIAG_PROFILE_ENABLE in rules.mk.
 * Each section keeps its longest run over the last second, in microseconds. */
enum {
    DIAG_SEC_BT_TRANSPORT, // ckbt51 UART task
    DIAG_SEC_BT_EVENTS,    // Bluetooth event queue
    DIAG_SEC_REPORT_BUF,   // report buffer
    DIAG_SEC_INDICATOR,    // indicators
    DIAG_SEC_BATTERY,      // battery measurement
    DIAG_SEC_LPM,          // low-power checks
    DIAG_SEC_MATRIX,       // key matrix scan
    DIAG_SEC_LED_FLUSH,    // LED flush step
    DIAG_SEC_BT_SEND,      // ckbt51_send_cmd (wake pulse + UART write)
    DIAG_SEC_COUNT
};

#ifdef DIAG_PROFILE_ENABLE
#    include "hal.h"
void diag_prof_add(uint8_t section, uint32_t start_cycles);
#    define DIAG_PROFILE(section, ...)          \
        do {                                    \
            uint32_t _diag_t0 = DWT->CYCCNT;    \
            __VA_ARGS__;                        \
            diag_prof_add(section, _diag_t0);   \
        } while (0)
#else
#    define DIAG_PROFILE(section, ...) \
        do {                           \
            __VA_ARGS__;               \
        } while (0)
#endif

void diag_task(void);
void diag_rx(uint8_t *data, uint8_t length);
