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

#pragma once

/* ===========================================================================
 * Community firmware tuning (see /readme.md). Every option can be overridden
 * from a keymap config.h (#undef then #define).
 * =========================================================================== */

/* Debounce window in ms. With DEBOUNCE_TYPE = sym_eager_pk (rules.mk) a press is
 * reported on the first contact; this window only filters the bounces that follow.
 * Raise to 7-8 if a worn switch double-types. Stock Keychron: sym_defer_g, 5 ms
 * (every press delayed by 5 ms). */
#ifndef DEBOUNCE
#    define DEBOUNCE 5
#endif

/* Send the LED frame to the LED drivers a few I2C transfers per main-loop pass instead
 * of all at once (several ms), so key scanning is never paused for long. */
#define SNLED27351_INCREMENTAL_FLUSH
/* 16-byte I2C transfers per main-loop pass: 1 keeps every pass under ~1.8 ms (measured)
 * with no visible change to the animations. */
#ifndef SNLED27351_FLUSH_CHUNKS_PER_STEP
#    define SNLED27351_FLUSH_CHUNKS_PER_STEP 1
#endif

#ifdef KC_BLUETOOTH_ENABLE
/* Skip the blocking 3 ms CKBT51 UART wake-up pulse for HID reports when a packet was
 * sent less than CKBT51_WAKE_SKIP_MS ago (the module only sleeps its UART after 3 s
 * of inactivity). Comment out to restore the stock behaviour. */
#    define CKBT51_SMART_WAKE
#    ifndef CKBT51_WAKE_SKIP_MS
#        define CKBT51_WAKE_SKIP_MS 1500
#    endif

/* Bluetooth report rate in Hz. The CKBT51 accepts 90 (stock) or 133. */
#    ifndef CKBT51_REPORT_RATE
#        define CKBT51_REPORT_RATE 133
#    endif

/* Backlight off delay on battery, Bluetooth connected, in seconds (stock: 600). */
#    ifndef CONNECTED_BACKLIGHT_DISABLE_TIMEOUT
#        define CONNECTED_BACKLIGHT_DISABLE_TIMEOUT 180
#    endif

/* Fn+B battery animation: time per 10 % step in ms (stock: 150, i.e. 1.5 s to fill). */
#    ifndef BAT_LEVEL_GROWING_INTERVAL
#        define BAT_LEVEL_GROWING_INTERVAL 50
#    endif

/* Keep the keys typed while the keyboard reconnects (e.g. the key that woke it up)
 * and send them once connected, if that happens within this many ms. 0 = disabled. */
#    ifndef BT_WAKE_KEY_BUFFER_MS
#        define BT_WAKE_KEY_BUFFER_MS 3000
#    endif

/* Send the battery level to the host OS (shown in its Bluetooth settings).
 * Comment out to disable. */
#    define BT_REPORT_BATTERY_LEVEL
#endif

/* turn off effects when suspended */
#define RGB_MATRIX_SLEEP
#define LED_MATRIX_SLEEP

/* DIP switch for Mac/win OS switch */
#define DIP_SWITCH_PINS \
    { A8 }

/* Increase I2C speed to 1000 KHz */
#define I2C1_TIMINGR_PRESC 0U
#define I2C1_TIMINGR_SCLDEL 3U
#define I2C1_TIMINGR_SDADEL 0U
#define I2C1_TIMINGR_SCLH 15U
#define I2C1_TIMINGR_SCLL 51U

#ifdef KC_BLUETOOTH_ENABLE
/* Hardware configuration */
#    define USB_BT_MODE_SELECT_PIN A10

#    define CKBT51_RESET_PIN A9
#    define CKBT51_INT_INPUT_PIN A5
#    define BLUETOOTH_INT_INPUT_PIN A6

#    define USB_POWER_SENSE_PIN B1
#    define USB_POWER_CONNECTED_LEVEL 0

#    define BAT_LOW_LED_PIN A4
#    define BAT_LOW_LED_PIN_ON_STATE 1

#    define HOST_DEVICES_COUNT 3

#    if defined(RGB_MATRIX_ENABLE) || defined(LED_MATRIX_ENABLE)

#        define SNLED27351_SDB_PIN C14

#        define BT_INDCATION_LED_MATRIX_LIST \
            { 20, 21, 22 }

#        define BAT_LEVEL_LED_LIST \
            { 20, 21, 22, 23, 24, 25, 26, 27, 28, 29 }

/* Backlit disable timeout when keyboard is disconnected(unit: second) */
#        ifndef DISCONNECTED_BACKLIGHT_DISABLE_TIMEOUT
#            define DISCONNECTED_BACKLIGHT_DISABLE_TIMEOUT 40
#        endif

/* Backlit disable timeout when keyboard is connected(unit: second) */
#        ifndef CONNECTED_BACKLIGHT_DISABLE_TIMEOUT
#            define CONNECTED_BACKLIGHT_DISABLE_TIMEOUT 600
#        endif
#    endif

/* Keep USB connection in blueooth mode */
#    define KEEP_USB_CONNECTION_IN_BLUETOOTH_MODE

#endif

/* Emulated EEPROM configuration */
#define WEAR_LEVELING_LOGICAL_SIZE 2048
#define WEAR_LEVELING_BACKING_SIZE (WEAR_LEVELING_LOGICAL_SIZE * 2)
#define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR 2047

/* Factory test keys */
#define FN_KEY1 MO(1)
#define FN_KEY2 MO(3)
