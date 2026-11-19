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

#include <string.h>
#include "quantum.h"
#include "raw_hid.h"
#include "diag.h"
#include "transport.h"
#include "bluetooth.h"
#include "battery.h"
#include "lpm.h"
#ifdef SNLED27351_INCREMENTAL_FLUSH
/* snled27351.c or snled27351-mono.c, depending on the variant */
void        snled27351_flush_task(void);
extern bool snled27351_incremental_flush;
extern uint8_t snled27351_flush_chunks;
#endif

#define DIAG_PROTOCOL_VERSION 1
#define DIAG_PROBE_HOLD_MS 20

enum {
    DIAG_CMD_INFO     = 0x01,
    DIAG_CMD_COUNTERS = 0x02,
    DIAG_CMD_RESET    = 0x03,
    DIAG_CMD_PROBE    = 0x04,
    DIAG_CMD_OPTION   = 0x05,
    DIAG_CMD_PROFILE  = 0x06,
};

diag_counters_t diag;

#ifdef CKBT51_SMART_WAKE
extern uint16_t ckbt51_wake_skip_ms;
#endif

#if defined(NKRO_ENABLE) && defined(BLUETOOTH_NKRO_ENABLE)
extern nkro_t nkro;
#endif

static uint32_t scan_count     = 0;
static uint32_t scan_window    = 0;
static uint16_t scan_rate      = 0;
static uint32_t last_pass      = 0;
static uint8_t  gap_max        = 0, gap_max_window = 0;   // longest main-loop pass, ms
static uint16_t slow_count     = 0, slow_window = 0;      // passes of 3 ms or more
static uint32_t probe_released = 0; // timestamp of the probe press, 0 = idle

#ifdef DIAG_PROFILE_ENABLE
#    define CYCLES_PER_US (STM32_SYSCLK / 1000000U)
static uint16_t prof_max[DIAG_SEC_COUNT], prof_max_window[DIAG_SEC_COUNT];
static uint32_t pass_max_us = 0, pass_max_window = 0, pass_start = 0;
static bool     prof_ready = false;

static void diag_prof_init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    prof_ready = true;
}

void diag_prof_add(uint8_t section, uint32_t start_cycles) {
    uint32_t us = (DWT->CYCCNT - start_cycles) / CYCLES_PER_US;
    if (us > 0xFFFF) us = 0xFFFF;
    if (section < DIAG_SEC_COUNT && us > prof_max_window[section]) prof_max_window[section] = us;
}

static void diag_prof_pass(bool window_end) {
    if (!prof_ready) diag_prof_init();
    uint32_t now = DWT->CYCCNT;
    if (pass_start) {
        uint32_t us = (now - pass_start) / CYCLES_PER_US;
        if (us > pass_max_window) pass_max_window = us;
    }
    pass_start = now;
    if (window_end) {
        memcpy(prof_max, prof_max_window, sizeof(prof_max));
        memset(prof_max_window, 0, sizeof(prof_max_window));
        pass_max_us     = pass_max_window;
        pass_max_window = 0;
    }
}
#endif

static void put16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = v >> 8;
}

static void put32(uint8_t *p, uint32_t v) {
    for (uint8_t i = 0; i < 4; i++) p[i] = (v >> (8 * i)) & 0xFF;
}

void diag_task(void) {
    uint32_t now = timer_read32();
    if (last_pass) {
        uint32_t gap = now - last_pass;
        if (gap > gap_max_window) gap_max_window = gap > 255 ? 255 : gap;
        if (gap >= 3) slow_window++;
    }
    last_pass = now;

    scan_count++;
    if (scan_window == 0) scan_window = sync_timer_read32() | 1;
    uint32_t elapsed = sync_timer_elapsed32(scan_window);
    if (elapsed >= 1000) {
        scan_rate   = scan_count * 1000 / elapsed;
        scan_count  = 0;
        gap_max        = gap_max_window;
        gap_max_window = 0;
        slow_count     = slow_window;
        slow_window    = 0;
        scan_window = sync_timer_read32() | 1;
    }

#ifdef DIAG_PROFILE_ENABLE
    diag_prof_pass(elapsed >= 1000);
#endif

    if (probe_released && sync_timer_elapsed32(probe_released) >= DIAG_PROBE_HOLD_MS) {
        probe_released = 0;
        unregister_code(KC_F24);
    }
}

void diag_rx(uint8_t *data, uint8_t length) {
    uint8_t  arg0 = data[3], arg1 = data[4]; // before out[] is cleared
    (void)arg1;
    uint8_t *out = &data[3];
    memset(out, 0, length - 3);
    data[2] = 0;

    switch (data[1]) {
        case DIAG_CMD_INFO: {
            out[0] = DIAG_PROTOCOL_VERSION;
            out[1] = get_transport();
            out[2] = bluetooth_get_state();
            out[3] = battery_get_percentage();
            put16(&out[4], battery_get_voltage());
            out[6] = usb_power_connected();
            put32(&out[7], timer_read32());
            put16(&out[11], scan_rate);
#ifdef CKBT51_REPORT_RATE
            put16(&out[13], CKBT51_REPORT_RATE);
#endif
            out[15] = DEBOUNCE;
            out[16] = 0
#ifdef CKBT51_SMART_WAKE
                      | 0x01
#endif
#ifdef BT_REPORT_BATTERY_LEVEL
                      | 0x02
#endif
#if defined(NKRO_ENABLE) && defined(BLUETOOTH_NKRO_ENABLE)
                      | (nkro.bluetooth ? 0x04 : 0)
#endif
                ;
            put32(&out[17], last_input_activity_elapsed());
            out[21] = gap_max;
            put16(&out[22], slow_count);
            break;
        }
        case DIAG_CMD_COUNTERS: {
            const uint32_t *c = (const uint32_t *)&diag;
            for (uint8_t i = 0; i < sizeof(diag) / sizeof(uint32_t); i++) put32(&out[4 * i], c[i]);
            break;
        }
        case DIAG_CMD_RESET:
            memset(&diag, 0, sizeof(diag));
            break;
        case DIAG_CMD_PROBE:
            if (probe_released) {
                data[2] = 1; // busy
                break;
            }
            out[0] = get_transport();
            /* Answer first, so the host timestamps the request before the key report. */
            raw_hid_send(data, length);
            register_code(KC_F24);
            probe_released = sync_timer_read32() | 1;
            return;
        case DIAG_CMD_OPTION: // [3] = option, [4] = value (0xFF = read); answers the value in [3]
            switch (arg0) {
#ifdef SNLED27351_INCREMENTAL_FLUSH
                case 1:
                    if (arg1 <= 1) snled27351_incremental_flush = arg1; // 0xFF = read only
                    out[0] = snled27351_incremental_flush;
                    break;
#endif
#ifdef CKBT51_SMART_WAKE
                case 2: // wake-pulse skip window, in 100 ms units
                    if (arg1 != 0xFF) ckbt51_wake_skip_ms = (uint16_t)arg1 * 100;
                    out[0] = ckbt51_wake_skip_ms / 100;
                    break;
#endif
#ifdef SNLED27351_INCREMENTAL_FLUSH
                case 3: // LED flush transfers per main-loop pass
                    if (arg1 >= 1 && arg1 <= 24) snled27351_flush_chunks = arg1;
                    out[0] = snled27351_flush_chunks;
                    break;
#endif
                default:
                    data[2] = 2; // unknown option
                    break;
            }
            break;
        case DIAG_CMD_PROFILE: // [0..3] longest pass, then one uint16 per section (us)
#ifdef DIAG_PROFILE_ENABLE
            put32(&out[0], pass_max_us);
            out[4] = DIAG_SEC_COUNT;
            for (uint8_t s = 0; s < DIAG_SEC_COUNT; s++) put16(&out[5 + 2 * s], prof_max[s]);
#else
            data[2] = 2; // not built in
#endif
            break;
        default:
            data[2] = 0xFF; // unknown command
            break;
    }

    raw_hid_send(data, length);
}
