/* Copyright 2023 @ Keychron (https://www.keychron.com)
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

/* Send the battery level to the host OS (shown in its Bluetooth settings).
 * Comment out to disable. */
#    define BT_REPORT_BATTERY_LEVEL
#endif

/* turn off effects when suspended */
#define RGB_DISABLE_WHEN_USB_SUSPENDED
#define LED_DISABLE_WHEN_USB_SUSPENDED

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

#        define LED_DRIVER_SHUTDOWN_PIN C14

#        define HOST_LED_MATRIX_LIST \
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

/* Enable bluetooth NKRO */
#    define BLUETOOTH_NKRO_ENABLE

/* Raw hid command for factory test and bluetooth DFU */
#    define RAW_HID_CMD 0xAA ... 0xAB
#else
/* Raw hid command for factory test */
#    define RAW_HID_CMD 0xAB
#endif

/* Emulated EEPROM configuration */
#define FEE_DENSITY_BYTES FEE_PAGE_SIZE
#define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR 2047

/* Factory test keys */
#define FN_KEY1 MO(1)
#define FN_KEY2 MO(3)
