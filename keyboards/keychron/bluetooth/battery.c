/* Copyright 2022 ~ 2025 @ lokher (https://www.keychron.com)
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

#include "quantum.h"
#include "bluetooth.h"
#include "battery.h"
#include "transport.h"
#include "ckbt51.h"
#include "lpm.h"
#include "indicator.h"
#include "rtc_timer.h"

#define BATTERY_EMPTY_COUNT 10
#define CRITICAL_LOW_COUNT 20

static uint32_t bat_monitor_timer_buffer = 0;
static uint16_t voltage                  = FULL_VOLTAGE_VALUE;
static uint8_t  bat_empty                = 0;
static uint8_t  critical_low             = 0;
static uint8_t  bat_state;
static uint8_t  power_on_sample = 0;

void battery_init(void) {
    bat_state = BAT_NOT_CHARGING;
}
__attribute__((weak)) void battery_measure(void) {
    ckbt51_read_state_reg(0x05, 0x02);
}

/* Calculate the voltage */
__attribute__((weak)) void battery_calculate_voltage(uint16_t value) {}

#ifdef BT_REPORT_BATTERY_LEVEL
#    ifndef BT_REPORT_BATTERY_LEVEL_HYSTERESIS
#        define BT_REPORT_BATTERY_LEVEL_HYSTERESIS 2
#    endif
static bool    voltage_measured      = false;
static uint8_t last_reported_bat_lvl  = 0xFF;

/* Force a battery level report on the next measurement (called on connection) */
void battery_report_level_reset(void) {
    last_reported_bat_lvl = 0xFF;
}

/* IMPROVEMENT: send the battery level to the host when connected, once after each
 * connection and then only when it moved by at least the hysteresis (the LED load
 * compensation makes the reading jitter by a point or two). */
static void battery_report_level(void) {
    if (!voltage_measured) return;

    uint8_t pct = battery_get_percentage();
    if (last_reported_bat_lvl == 0xFF || pct >= last_reported_bat_lvl + BT_REPORT_BATTERY_LEVEL_HYSTERESIS || pct + BT_REPORT_BATTERY_LEVEL_HYSTERESIS <= last_reported_bat_lvl || (pct == 0 && last_reported_bat_lvl != 0) || (pct == 100 && last_reported_bat_lvl != 100)) {
        last_reported_bat_lvl = pct;
        ckbt51_update_bat_lvl(pct);
    }
}
#endif

void battery_set_voltage(uint16_t value) {
    voltage = value;
#ifdef BT_REPORT_BATTERY_LEVEL
    voltage_measured = true;
#endif
}

uint16_t battery_get_voltage(void) {
    return voltage;
}

/* IMPROVEMENT: the stock code mapped 3.5-4.1 V linearly to 20-100 %, which is far from
 * a real Li-ion discharge curve (the long plateau around 3.7-3.8 V holds most of the
 * capacity), so the displayed level dropped fast at first and then stalled.
 * Typical 1S Li-ion curve under light load, linearly interpolated between points.
 * The low battery / shutdown thresholds (EMPTY/SHUTDOWN_VOLTAGE_VALUE) are unchanged. */
typedef struct {
    uint16_t mv;
    uint8_t  percent;
} battery_curve_point_t;

static const battery_curve_point_t battery_curve[] = {
    {4150, 100}, {4050, 90}, {3970, 80}, {3900, 70}, {3840, 60}, {3800, 50},
    {3770, 40},  {3740, 30}, {3700, 20}, {3620, 10}, {3500, 5},  {3300, 0},
};

uint8_t battery_get_percentage(void) {
    const uint8_t n = sizeof(battery_curve) / sizeof(battery_curve[0]);

    if (voltage >= battery_curve[0].mv) return 100;
    if (voltage <= battery_curve[n - 1].mv) return 0;

    for (uint8_t i = 1; i < n; i++) {
        if (voltage >= battery_curve[i].mv) {
            const battery_curve_point_t *hi = &battery_curve[i - 1];
            const battery_curve_point_t *lo = &battery_curve[i];
            return lo->percent + (uint32_t)(voltage - lo->mv) * (hi->percent - lo->percent) / (hi->mv - lo->mv);
        }
    }
    return 0;
}

bool battery_is_empty(void) {
    return bat_empty > BATTERY_EMPTY_COUNT;
}

bool battery_is_critical_low(void) {
    return critical_low > CRITICAL_LOW_COUNT;
}

void battery_check_empty(void) {
    if (voltage < EMPTY_VOLTAGE_VALUE) {
        if (bat_empty <= BATTERY_EMPTY_COUNT) {
            if (++bat_empty > BATTERY_EMPTY_COUNT) {
#if defined(BAT_LOW_LED_PIN) || defined(BAT_LOW_LED_PIN_STATE)
                indicator_battery_low_enable(true);
#endif
#if defined(LOW_BAT_IND_INDEX)
                indicator_battery_low_backlit_enable(true);
#endif
                power_on_sample = VOLTAGE_POWER_ON_MEASURE_COUNT;
            }
        }
    }
}

void battery_check_critical_low(void) {
    if (voltage < SHUTDOWN_VOLTAGE_VALUE) {
        if (critical_low <= CRITICAL_LOW_COUNT) {
            if (++critical_low > CRITICAL_LOW_COUNT) bluetooth_low_battery_shutdown();
        }
    } else if (critical_low <= CRITICAL_LOW_COUNT) {
        critical_low = 0;
    }
}

bool battery_power_on_sample(void) {
    return power_on_sample < VOLTAGE_POWER_ON_MEASURE_COUNT;
}

void battery_task(void) {
    uint32_t t = rtc_timer_elapsed_ms(bat_monitor_timer_buffer);
    if (get_transport() == TRANSPORT_BLUETOOTH && bluetooth_get_state() == BLUETOOTH_CONNECTED) {
#ifdef BT_REPORT_BATTERY_LEVEL
        battery_report_level();
#endif
        if ((battery_power_on_sample()
#if defined(LED_MATRIX_ENABLE) || defined(RGB_MATRIX_ENABLE)
             && !indicator_is_enabled()
#endif
             && t > BACKLIGHT_OFF_VOLTAGE_MEASURE_INTERVAL) ||
            t > VOLTAGE_MEASURE_INTERVAL) {

            battery_check_empty();
            battery_check_critical_low();

            bat_monitor_timer_buffer = rtc_timer_read_ms();
            if (bat_monitor_timer_buffer > RTC_MAX_TIME) {
                bat_monitor_timer_buffer = 0;
                rtc_timer_clear();
            }

            battery_measure();
            power_on_sample++;
            if (power_on_sample > VOLTAGE_POWER_ON_MEASURE_COUNT) power_on_sample = VOLTAGE_POWER_ON_MEASURE_COUNT;
        }
    }

    if ((bat_empty || critical_low) && usb_power_connected()) {
        bat_empty    = false;
        critical_low = false;
#if defined(BAT_LOW_LED_PIN) || defined(BAT_LOW_LED_PIN_STATE)
        indicator_battery_low_enable(false);
#endif
#if defined(LOW_BAT_IND_INDEX)
        indicator_battery_low_backlit_enable(false);
#endif
    }
}
