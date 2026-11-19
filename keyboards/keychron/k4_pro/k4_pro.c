/* Copyright 2023 ~ 2025 @ Keychron (https://www.keychron.com)
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

#include "k4_pro.h"
#ifdef KC_BLUETOOTH_ENABLE
#    include "ckbt51.h"
#    include "bluetooth.h"
#    include "indicator.h"
#    include "transport.h"
#    include "battery.h"
#    include "bat_level_animation.h"
#    include "lpm.h"
#endif

#ifdef ENABLE_FACTORY_TEST
#    include "factory_test.h"
#endif

#define POWER_ON_LED_DURATION 3000

typedef struct PACKED {
    uint8_t len;
    uint8_t keycode[3];
} key_combination_t;

static uint32_t power_on_indicator_timer_buffer;
static uint32_t siri_timer_buffer = 0;
static uint8_t  mac_keycode[4]    = {KC_LOPT, KC_ROPT, KC_LCMD, KC_RCMD};

key_combination_t key_comb_list[4] = {
    {2, {KC_LWIN, KC_TAB}},        // Task (win)
    {2, {KC_LWIN, KC_E}},          // Files (win)
    {3, {KC_LSFT, KC_LGUI, KC_4}}, // Snapshot (mac)
    {2, {KC_LWIN, KC_C}}           // Cortana (win)
};

#ifdef KC_BLUETOOTH_ENABLE
bool                   firstDisconnect  = true;
bool                   bt_factory_reset = false;

/* FIX: the original code started pairing from a ChibiOS virtual-timer callback, i.e. in
 * interrupt context, which then busy-waited 3 ms and wrote to the UART (sdWrite is not
 * ISR-safe) while racing with the main loop on bt_state. The long press is now timed
 * with a plain timestamp and handled from matrix_scan_kb() in the main loop. */
#    define PAIRING_KEY_HOLD_MS 2000
static uint32_t pairing_key_timer = 0;
static uint8_t  pairing_host_idx  = 0;

#    ifndef CKBT51_REPORT_RATE
#        define CKBT51_REPORT_RATE 90
#    endif
#endif

bool dip_switch_update_kb(uint8_t index, bool active) {
    if (index == 0) {
        default_layer_set(1UL << (active ? 0 : 2));
    }
    dip_switch_update_user(index, active);

    return true;
}

#ifdef KC_BLUETOOTH_ENABLE
bool process_record_kb_bt(uint16_t keycode, keyrecord_t *record) {
#else
bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
#endif
    static uint8_t host_idx = 0;

    switch (keycode) {
        case KC_LOPTN:
        case KC_ROPTN:
        case KC_LCMMD:
        case KC_RCMMD:
            if (record->event.pressed) {
                register_code(mac_keycode[keycode - KC_LOPTN]);
            } else {
                unregister_code(mac_keycode[keycode - KC_LOPTN]);
            }
            return false; // Skip all further processing of this key)
        case KC_MCTRL:
            if (record->event.pressed) {
                register_code(KC_MISSION_CONTROL);
            } else {
                unregister_code(KC_MISSION_CONTROL);
            }
            return false;
        case KC_LNPAD:
            if (record->event.pressed) {
                register_code(KC_LAUNCHPAD);
            } else {
                unregister_code(KC_LAUNCHPAD);
            }
            return false;
        case KC_TASK:
        case KC_FILE:
        case KC_SNAP:
        case KC_CTANA:
            if (record->event.pressed) {
                for (uint8_t i = 0; i < key_comb_list[keycode - KC_TASK].len; i++)
                    register_code(key_comb_list[keycode - KC_TASK].keycode[i]);
            } else {
                for (uint8_t i = 0; i < key_comb_list[keycode - KC_TASK].len; i++)
                    unregister_code(key_comb_list[keycode - KC_TASK].keycode[i]);
            }
            return false; // Skip all further processing of this key
        case KC_SIRI:
            if (record->event.pressed && siri_timer_buffer == 0) {
                register_code(KC_LGUI);
                register_code(KC_SPACE);
                siri_timer_buffer = sync_timer_read32() | 1;
            }
            return false; // Skip all further processing of this key
#ifdef KC_BLUETOOTH_ENABLE
        case BT_HST1 ... BT_HST3:
            if (get_transport() == TRANSPORT_BLUETOOTH) {
                if (record->event.pressed) {
                    host_idx          = keycode - BT_HST1 + 1;
                    pairing_host_idx  = host_idx;
                    pairing_key_timer = sync_timer_read32() | 1;
                    bluetooth_connect_ex(host_idx, 0);
                } else {
                    host_idx          = 0;
                    pairing_key_timer = 0;
                }
            }
            break;
        case BAT_LVL:
            if (get_transport() == TRANSPORT_BLUETOOTH && !usb_power_connected()) {
                bat_level_animiation_start(battery_get_percentage());
            }
            break;
#endif
        default:
#ifdef FACTORY_RESET_CHECK
            FACTORY_RESET_CHECK(keycode, record);
#endif
            break;
    }
    return true;
}

void keyboard_post_init_kb(void) {
    dip_switch_read(true);

#ifdef KC_BLUETOOTH_ENABLE
    /* Currently we don't use this reset pin */
    palSetLineMode(CKBT51_RESET_PIN, PAL_MODE_UNCONNECTED);

    /* IMPORTANT: DO NOT enable internal pull-up resistor
     * as there is an external pull-down resistor.
     */
    palSetLineMode(USB_BT_MODE_SELECT_PIN, PAL_MODE_INPUT);

    ckbt51_init(false);
    bluetooth_init();
#endif

    power_on_indicator_timer_buffer = sync_timer_read32() | 1;
    gpio_write_pin(BAT_LOW_LED_PIN, BAT_LOW_LED_PIN_ON_STATE);

    keyboard_post_init_user();
}

void matrix_scan_kb(void) {
    if (power_on_indicator_timer_buffer) {
        if (sync_timer_elapsed32(power_on_indicator_timer_buffer) > POWER_ON_LED_DURATION) {
            power_on_indicator_timer_buffer = 0;

            gpio_write_pin(BAT_LOW_LED_PIN, !BAT_LOW_LED_PIN_ON_STATE);

        } else {
            gpio_write_pin(BAT_LOW_LED_PIN, BAT_LOW_LED_PIN_ON_STATE);
        }
    }

    if (siri_timer_buffer && sync_timer_elapsed32(siri_timer_buffer) > 500) {
        siri_timer_buffer = 0;
        unregister_code(KC_LGUI);
        unregister_code(KC_SPACE);
    }

#ifdef KC_BLUETOOTH_ENABLE
    if (pairing_key_timer && sync_timer_elapsed32(pairing_key_timer) > PAIRING_KEY_HOLD_MS) {
        pairing_key_timer = 0;
        bluetooth_pairing_ex(pairing_host_idx, NULL);
    }
#endif

#ifdef FACTORY_RESET_TASK
    FACTORY_RESET_TASK();
#endif
    matrix_scan_user();
}

#ifdef KC_BLUETOOTH_ENABLE
static void ckbt51_param_init(void) {
    /* Set bluetooth device name */
    // ckbt51_set_local_name(STR(PRODUCT));
    ckbt51_set_local_name(PRODUCT);
    /* Set bluetooth parameters */
    module_param_t param = {.event_mode             = 0x02,
                            .connected_idle_timeout = 7200,
                            .pairing_timeout        = 180,
                            .pairing_mode           = 0,
                            .reconnect_timeout      = 5,
                            .report_rate            = CKBT51_REPORT_RATE,
                            .vendor_id_source       = 1,
                            .verndor_id             = 0, // Must be 0x3434
                            .product_id             = PRODUCT_ID};
    ckbt51_set_param(&param);
}

void bluetooth_enter_disconnected_kb(uint8_t host_idx) {
    if (bt_factory_reset) {
        bt_factory_reset = false;
        ckbt51_param_init();
    }
    /* CKBT51 bluetooth module boot time is slower, it enters disconnected after boot,
       so we place initialization here. */
    if (firstDisconnect && sync_timer_read32() < 1000 && get_transport() == TRANSPORT_BLUETOOTH) {
        ckbt51_param_init();
        bluetooth_connect();
        firstDisconnect = false;
    }
}

void ckbt51_default_ack_handler(uint8_t *data, uint8_t len) {
    if (data[1] == 0x45) {
        module_param_t param = {.event_mode             = 0x02,
                                .connected_idle_timeout = 7200,
                                .pairing_timeout        = 180,
                                .pairing_mode           = 0,
                                .reconnect_timeout      = 5,
                                .report_rate            = CKBT51_REPORT_RATE,
                                .vendor_id_source       = 1,
                                .verndor_id             = 0, // Must be 0x3434
                                .product_id             = PRODUCT_ID};
        ckbt51_set_param(&param);
    }
}

/* FIX: the Cable/BT slider was "debounced" by reading the pin twice in a row, which
 * filters nothing: contact bounce while sliding could switch USB -> BT -> USB and restart
 * a connection. A new position is now applied only once it has been stable for
 * MODE_SWITCH_DEBOUNCE_MS. The very first read at boot is applied immediately. */
#    ifndef MODE_SWITCH_DEBOUNCE_MS
#        define MODE_SWITCH_DEBOUNCE_MS 20
#    endif

void bluetooth_pre_task(void) {
    static uint8_t  mode    = 1;
    static uint8_t  pending = 1;
    static uint32_t timer   = 0;
    static bool     first   = true;

    uint8_t now = gpio_read_pin(USB_BT_MODE_SELECT_PIN);

    if (first) {
        first = false;
        if (now != mode) {
            mode = now;
            set_transport(mode == 0 ? TRANSPORT_BLUETOOTH : TRANSPORT_USB);
        }
        return;
    }

    if (now == mode) {
        timer = 0;
        return;
    }
    if (timer == 0 || now != pending) {
        pending = now;
        timer   = sync_timer_read32() | 1;
        return;
    }
    if (sync_timer_elapsed32(timer) >= MODE_SWITCH_DEBOUNCE_MS) {
        timer = 0;
        mode  = now;
        set_transport(mode == 0 ? TRANSPORT_BLUETOOTH : TRANSPORT_USB);
    }
}
#endif

void battery_calculte_voltage(uint16_t value) {
    uint16_t voltage = ((uint32_t)value) * 2246 / 1000;

#ifdef LED_MATRIX_ENABLE
    if (led_matrix_is_enabled()) {
        uint32_t compensation = (30 * led_matrix_driver.get_load_ratio());
        voltage += compensation;
    }
#endif
#ifdef RGB_MATRIX_ENABLE
    if (rgb_matrix_is_enabled()) {
        /* We assumpt it is linear relationship*/
        uint32_t compensation = (60 * rgb_matrix_driver.get_load_ratio());
        voltage += compensation;
    }
#endif
    battery_set_voltage(voltage);
}

bool via_command_kb(uint8_t *data, uint8_t length) {
    switch (data[0]) {
#ifdef KC_BLUETOOTH_ENABLE
        case 0xAA:
            ckbt51_dfu_rx(data, length);
            break;
#endif
#ifdef ENABLE_FACTORY_TEST
        case 0xAB:
            factory_test_rx(data, length);
            break;
#endif
        default:
            return false;
    }

    return true;
}

#if !defined(VIA_ENABLE)
void raw_hid_receive(uint8_t *data, uint8_t length) {
    via_command_kb(data, length);
}
#endif
