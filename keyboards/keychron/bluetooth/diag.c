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

#define DIAG_PROTOCOL_VERSION 1
#define DIAG_PROBE_HOLD_MS 20

enum {
    DIAG_CMD_INFO     = 0x01,
    DIAG_CMD_COUNTERS = 0x02,
    DIAG_CMD_RESET    = 0x03,
    DIAG_CMD_PROBE    = 0x04,
};

diag_counters_t diag;

static uint32_t scan_count     = 0;
static uint32_t scan_window    = 0;
static uint16_t scan_rate      = 0;
static uint32_t probe_released = 0; // timestamp of the probe press, 0 = idle

static void put16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = v >> 8;
}

static void put32(uint8_t *p, uint32_t v) {
    for (uint8_t i = 0; i < 4; i++) p[i] = (v >> (8 * i)) & 0xFF;
}

void diag_task(void) {
    scan_count++;
    if (scan_window == 0) scan_window = sync_timer_read32() | 1;
    uint32_t elapsed = sync_timer_elapsed32(scan_window);
    if (elapsed >= 1000) {
        scan_rate   = scan_count * 1000 / elapsed;
        scan_count  = 0;
        scan_window = sync_timer_read32() | 1;
    }

    if (probe_released && sync_timer_elapsed32(probe_released) >= DIAG_PROBE_HOLD_MS) {
        probe_released = 0;
        unregister_code(KC_F24);
    }
}

void diag_rx(uint8_t *data, uint8_t length) {
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
        default:
            data[2] = 0xFF; // unknown command
            break;
    }

    raw_hid_send(data, length);
}
