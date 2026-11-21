/* Copyright 2021 @ Keychron (https://www.keychron.com)
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

#include "snled27351-mono.h"
#include "i2c_master.h"
#include "gpio.h"

#define SNLED27351_PWM_REGISTER_COUNT 192
#define SNLED27351_LED_CONTROL_REGISTER_COUNT 24

#ifndef SNLED27351_I2C_TIMEOUT
#    define SNLED27351_I2C_TIMEOUT 100
#endif

#ifndef SNLED27351_I2C_PERSISTENCE
#    define SNLED27351_I2C_PERSISTENCE 0
#endif

#ifndef SNLED27351_PHASE_CHANNEL
#    define SNLED27351_PHASE_CHANNEL SNLED27351_SCAN_PHASE_12_CHANNEL
#endif

#ifndef SNLED27351_CURRENT_TUNE
#    define SNLED27351_CURRENT_TUNE {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
#endif

const uint8_t i2c_addresses[SNLED27351_DRIVER_COUNT] = {
    SNLED27351_I2C_ADDRESS_1,
#ifdef SNLED27351_I2C_ADDRESS_2
    SNLED27351_I2C_ADDRESS_2,
#    ifdef SNLED27351_I2C_ADDRESS_3
    SNLED27351_I2C_ADDRESS_3,
#        ifdef SNLED27351_I2C_ADDRESS_4
    SNLED27351_I2C_ADDRESS_4,
#        endif
#    endif
#endif
};

// These buffers match the SNLED27351 PWM registers.
// The control buffers match the PG0 LED On/Off registers.
// Storing them like this is optimal for I2C transfers to the registers.
// We could optimize this and take out the unused registers from these
// buffers and the transfers in snled27351_write_pwm_buffer() but it's
// probably not worth the extra complexity.
typedef struct snled27351_driver_t {
    uint8_t pwm_buffer[SNLED27351_PWM_REGISTER_COUNT];
    bool    pwm_buffer_dirty;
    uint8_t led_control_buffer[SNLED27351_LED_CONTROL_REGISTER_COUNT];
    bool    led_control_buffer_dirty;
} PACKED snled27351_driver_t;

snled27351_driver_t driver_buffers[SNLED27351_DRIVER_COUNT] = {{
    .pwm_buffer               = {0},
    .pwm_buffer_dirty         = false,
    .led_control_buffer       = {0},
    .led_control_buffer_dirty = false,
}};

#ifdef SNLED27351_INCREMENTAL_FLUSH
/* FIX: the incremental flush sends one 16-byte register line per main-loop pass, and each
 * line holds a single colour channel of up to 16 LEDs. When most of the frame changes at
 * once (backlight back on after sleep, start or end of a Bluetooth indication), the LEDs
 * showed the half-written frame for tens of ms: rows lit in green first (the G line is
 * sent before R and B), then cyan, then white. Such frames are now sent in one block.
 * A LED "jumps" when a channel changes by SNLED27351_JUMP_DELTA or more since the last
 * flush; SNLED27351_JUMP_LEDS jumping LEDs make the frame go out in one block. */
#    ifndef SNLED27351_JUMP_DELTA
#        define SNLED27351_JUMP_DELTA 64
#    endif
#    ifndef SNLED27351_JUMP_LEDS
#        define SNLED27351_JUMP_LEDS 16
#    endif
static uint8_t flush_jumps = 0; // LEDs that jumped since the last snled27351_flush()

static inline bool snled27351_jump(uint8_t from, uint8_t to) {
    return (from > to ? from - to : to - from) >= SNLED27351_JUMP_DELTA;
}

static inline void snled27351_count_jump(bool jumped) {
    if (jumped && flush_jumps < UINT8_MAX) flush_jumps++;
}
#endif

void snled27351_write_register(uint8_t index, uint8_t reg, uint8_t data) {
#if SNLED27351_I2C_PERSISTENCE > 0
    for (uint8_t i = 0; i < SNLED27351_I2C_PERSISTENCE; i++) {
        if (i2c_write_register(i2c_addresses[index] << 1, reg, &data, 1, SNLED27351_I2C_TIMEOUT) == I2C_STATUS_SUCCESS) break;
    }
#else
    i2c_write_register(i2c_addresses[index] << 1, reg, &data, 1, SNLED27351_I2C_TIMEOUT);
#endif
}

void snled27351_select_page(uint8_t index, uint8_t page) {
    snled27351_write_register(index, SNLED27351_REG_COMMAND, page);
}

void snled27351_write_pwm_buffer(uint8_t index) {
    // Assumes PG1 is already selected.
    // Transmit PWM registers in 12 transfers of 16 bytes.

    // Iterate over the pwm_buffer contents at 16 byte intervals.
    for (uint8_t i = 0; i < SNLED27351_PWM_REGISTER_COUNT; i += 16) {
#if SNLED27351_I2C_PERSISTENCE > 0
        for (uint8_t j = 0; j < SNLED27351_I2C_PERSISTENCE; j++) {
            if (i2c_write_register(i2c_addresses[index] << 1, i, driver_buffers[index].pwm_buffer + i, 16, SNLED27351_I2C_TIMEOUT) == I2C_STATUS_SUCCESS) break;
        }
#else
        i2c_write_register(i2c_addresses[index] << 1, i, driver_buffers[index].pwm_buffer + i, 16, SNLED27351_I2C_TIMEOUT);
#endif
    }
}

void snled27351_init_drivers(void) {
    i2c_init();

#if defined(SNLED27351_SDB_PIN)
    gpio_set_pin_output(SNLED27351_SDB_PIN);
    gpio_write_pin_high(SNLED27351_SDB_PIN);
#endif

    for (uint8_t i = 0; i < SNLED27351_DRIVER_COUNT; i++) {
        snled27351_init(i);
    }

    for (int i = 0; i < SNLED27351_LED_COUNT; i++) {
        snled27351_set_led_control_register(i, true);
    }

    for (uint8_t i = 0; i < SNLED27351_DRIVER_COUNT; i++) {
        snled27351_update_led_control_registers(i);
    }
}

void snled27351_init(uint8_t index) {
    snled27351_select_page(index, SNLED27351_COMMAND_FUNCTION);

    // Setting LED driver to shutdown mode
    snled27351_write_register(index, SNLED27351_FUNCTION_REG_SOFTWARE_SHUTDOWN, SNLED27351_SOFTWARE_SHUTDOWN_SSD_SHUTDOWN);
    // Setting internal channel pulldown/pullup
    snled27351_write_register(index, SNLED27351_FUNCTION_REG_PULLDOWNUP, SNLED27351_PULLDOWNUP_ALL_ENABLED);
    // Select number of scan phase
    snled27351_write_register(index, SNLED27351_FUNCTION_REG_SCAN_PHASE, SNLED27351_PHASE_CHANNEL);
    // Setting PWM Delay Phase
    snled27351_write_register(index, SNLED27351_FUNCTION_REG_SLEW_RATE_CONTROL_MODE_1, SNLED27351_SLEW_RATE_CONTROL_MODE_1_PDP_ENABLE);
    // Setting Driving/Sinking Channel Slew Rate
    snled27351_write_register(index, SNLED27351_FUNCTION_REG_SLEW_RATE_CONTROL_MODE_2, SNLED27351_SLEW_RATE_CONTROL_MODE_2_DSL_ENABLE | SNLED27351_SLEW_RATE_CONTROL_MODE_2_SSL_ENABLE);
    // Setting Iref
    snled27351_write_register(index, SNLED27351_FUNCTION_REG_SOFTWARE_SLEEP, 0);

    snled27351_select_page(index, SNLED27351_COMMAND_LED_CONTROL);

    for (int i = 0; i < SNLED27351_LED_CONTROL_ON_OFF_LENGTH; i++) {
        snled27351_write_register(index, i, 0x00);
    }

    snled27351_select_page(index, SNLED27351_COMMAND_PWM);

    for (int i = 0; i < SNLED27351_LED_CURRENT_TUNE_LENGTH; i++) {
        snled27351_write_register(index, i, 0x00);
    }

    snled27351_select_page(index, SNLED27351_COMMAND_CURRENT_TUNE);

    uint8_t current_tune_reg_list[SNLED27351_LED_CURRENT_TUNE_LENGTH] = SNLED27351_CURRENT_TUNE;
    for (int i = 0; i < SNLED27351_LED_CURRENT_TUNE_LENGTH; i++) {
        snled27351_write_register(index, i, current_tune_reg_list[i]);
    }

    snled27351_select_page(index, SNLED27351_COMMAND_LED_CONTROL);

    for (int i = 0; i < SNLED27351_LED_CONTROL_ON_OFF_LENGTH; i++) {
        snled27351_write_register(index, i, 0xFF);
    }

    snled27351_select_page(index, SNLED27351_COMMAND_FUNCTION);

    // Setting LED driver to normal mode
    snled27351_write_register(index, SNLED27351_FUNCTION_REG_SOFTWARE_SHUTDOWN, SNLED27351_SOFTWARE_SHUTDOWN_SSD_NORMAL);
}

void snled27351_set_value(int index, uint8_t value) {
    snled27351_led_t led;
    if (index >= 0 && index < SNLED27351_LED_COUNT) {
        memcpy_P(&led, (&g_snled27351_leds[index]), sizeof(led));

        if (driver_buffers[led.driver].pwm_buffer[led.v] == value) {
            return;
        }

#ifdef SNLED27351_INCREMENTAL_FLUSH
        snled27351_count_jump(snled27351_jump(driver_buffers[led.driver].pwm_buffer[led.v], value));
#endif
        driver_buffers[led.driver].pwm_buffer[led.v] = value;
        driver_buffers[led.driver].pwm_buffer_dirty  = true;
    }
}

void snled27351_set_value_all(uint8_t value) {
    for (int i = 0; i < SNLED27351_LED_COUNT; i++) {
        snled27351_set_value(i, value);
    }
}

void snled27351_set_led_control_register(uint8_t index, bool value) {
    snled27351_led_t led;
    memcpy_P(&led, (&g_snled27351_leds[index]), sizeof(led));

    uint8_t control_register = led.v / 8;
    uint8_t bit_value        = led.v % 8;

    if (value) {
        driver_buffers[led.driver].led_control_buffer[control_register] |= (1 << bit_value);
    } else {
        driver_buffers[led.driver].led_control_buffer[control_register] &= ~(1 << bit_value);
    }

    driver_buffers[led.driver].led_control_buffer_dirty = true;
}

float snled27351_get_load_ratio(void) {
    uint32_t totalBuf = 0;
     for (uint8_t i = 0; i < SNLED27351_DRIVER_COUNT; i++)
        for (uint8_t j = 0; j < SNLED27351_PWM_REGISTER_COUNT; j++)
            totalBuf += driver_buffers[i].pwm_buffer[j];

    return (float)totalBuf/(0xFF * 3 * SNLED27351_LED_COUNT);
}

void snled27351_update_pwm_buffers(uint8_t index) {
    if (driver_buffers[index].pwm_buffer_dirty) {
        snled27351_select_page(index, SNLED27351_COMMAND_PWM);

        snled27351_write_pwm_buffer(index);

        driver_buffers[index].pwm_buffer_dirty = false;
    }
}

void snled27351_update_led_control_registers(uint8_t index) {
    if (driver_buffers[index].led_control_buffer_dirty) {
        snled27351_select_page(index, SNLED27351_COMMAND_LED_CONTROL);

        for (uint8_t i = 0; i < SNLED27351_LED_CONTROL_REGISTER_COUNT; i++) {
            snled27351_write_register(index, i, driver_buffers[index].led_control_buffer[i]);
        }

        driver_buffers[index].led_control_buffer_dirty = false;
    }
}

#ifdef SNLED27351_INCREMENTAL_FLUSH
/* IMPROVEMENT: a full flush of both drivers is ~24 blocking I2C transfers (several ms),
 * during which the main loop does not scan the keys. The flush is now spread over the
 * following main-loop passes, SNLED27351_FLUSH_CHUNKS_PER_STEP transfers at a time, so
 * a key press is never held up by more than a fraction of a millisecond.
 * snled27351_flush_task() must be called from the main loop. */
#    ifndef SNLED27351_FLUSH_CHUNKS_PER_STEP
#        define SNLED27351_FLUSH_CHUNKS_PER_STEP 3
#    endif
static int8_t  flush_driver = -1; // driver being sent, -1 = idle
static uint8_t flush_offset = 0;
bool           snled27351_incremental_flush = true; // can be turned off at run time (A/B tests)
uint8_t        snled27351_flush_chunks      = SNLED27351_FLUSH_CHUNKS_PER_STEP; // 16-byte transfers per pass

static bool snled27351_flush_next_driver(void) {
    for (uint8_t i = (flush_driver < 0 ? 0 : flush_driver + 1); i < SNLED27351_DRIVER_COUNT; i++) {
        if (driver_buffers[i].pwm_buffer_dirty) {
            driver_buffers[i].pwm_buffer_dirty = false; // changes from now on mark it dirty again
            flush_driver                       = i;
            flush_offset                       = 0;
            return true;
        }
    }
    flush_driver = -1;
    return false;
}

void snled27351_flush_task(void) {
    if (flush_driver < 0) return;
    /* Re-select the PWM page every step: other writes may have changed it meanwhile. */
    snled27351_select_page(flush_driver, SNLED27351_COMMAND_PWM);
    for (uint8_t n = 0; n < snled27351_flush_chunks; n++) {
        i2c_write_register(i2c_addresses[flush_driver] << 1, flush_offset, driver_buffers[flush_driver].pwm_buffer + flush_offset, 16, SNLED27351_I2C_TIMEOUT);
        flush_offset += 16;
        if (flush_offset >= SNLED27351_PWM_REGISTER_COUNT) {
            if (!snled27351_flush_next_driver()) return;
            snled27351_select_page(flush_driver, SNLED27351_COMMAND_PWM);
        }
    }
}

static void snled27351_flush_complete(void) {
    while (flush_driver >= 0) snled27351_flush_task();
}

void snled27351_flush(void) {
    bool jumped = flush_jumps >= SNLED27351_JUMP_LEDS;
    flush_jumps = 0;
    if (!snled27351_incremental_flush || jumped) {
        snled27351_flush_complete();
        for (uint8_t i = 0; i < SNLED27351_DRIVER_COUNT; i++) snled27351_update_pwm_buffers(i);
        return;
    }
    if (flush_driver < 0 && !snled27351_flush_next_driver()) return;
    snled27351_flush_task();
}
#else
void snled27351_flush(void) {
    for (uint8_t i = 0; i < SNLED27351_DRIVER_COUNT; i++) {
        snled27351_update_pwm_buffers(i);
    }
}
#endif

void snled27351_shutdown(void) {
#ifdef SNLED27351_INCREMENTAL_FLUSH
    snled27351_flush_complete();
#endif
#    if defined(SN27351_SDB_PIN)
    gpio_write_pin_low(SN27351_SDB_PIN);
#    else
    for (uint8_t i = 0; i < SNLED27351_DRIVER_COUNT; i++)
        snled27351_sw_shutdown(i);
#    endif
}

void snled27351_exit_shutdown(void) {
#    if defined(SN27351_SDB_PIN)
    gpio_write_pin_high(SN27351_SDB_PIN);
#    else
    for (uint8_t i = 0; i < SNLED27351_DRIVER_COUNT; i++)
        snled27351_sw_return_normal(i);
#    endif
}

void snled27351_sw_return_normal(uint8_t index) {
    snled27351_select_page(index, SNLED27351_COMMAND_FUNCTION);

    // Setting LED driver to normal mode
    snled27351_write_register(index, SNLED27351_FUNCTION_REG_SOFTWARE_SHUTDOWN, SNLED27351_SOFTWARE_SHUTDOWN_SSD_NORMAL);
}

void snled27351_sw_shutdown(uint8_t index) {
    snled27351_select_page(index, SNLED27351_COMMAND_FUNCTION);

    // Setting LED driver to shutdown mode
    snled27351_write_register(index, SNLED27351_FUNCTION_REG_SOFTWARE_SHUTDOWN, SNLED27351_SOFTWARE_SHUTDOWN_SSD_SHUTDOWN);
    // Write SW Sleep Register
    snled27351_write_register(index, SNLED27351_FUNCTION_REG_SOFTWARE_SLEEP, SNLED27351_SOFTWARE_SLEEP_ENABLE);
}
