/* RGB LED driver (drivers/led/snled27351.c) with SNLED27351_INCREMENTAL_FLUSH, built
 * like the K4 Pro ISO RGB: two drivers, 101 LEDs, one 16-byte I2C transfer per pass.
 *
 * The fake I2C bus below keeps a copy of each chip's PWM page: that is what the LEDs
 * actually show between two main-loop passes. */
#define RGB_MATRIX_SNLED27351
#define RGB_MATRIX_LED_COUNT 101
#define SNLED27351_I2C_ADDRESS_1 SNLED27351_I2C_ADDRESS_VDDIO
#define SNLED27351_I2C_ADDRESS_2 SNLED27351_I2C_ADDRESS_GND
#define SNLED27351_INCREMENTAL_FLUSH
#define SNLED27351_FLUSH_CHUNKS_PER_STEP 1
#include "../../drivers/led/snled27351.c"

#include <stdio.h>
#include "harness.h"

/* ---- Fake chips ---------------------------------------------------------- */
static uint8_t chip_page[2];
static uint8_t chip_pwm[2][SNLED27351_PWM_REGISTER_COUNT];
static int     pwm_transfers;

static int chip_of(uint8_t devaddr) { return (devaddr >> 1) == SNLED27351_I2C_ADDRESS_1 ? 0 : 1; }

void i2c_init(void) {}
i2c_status_t i2c_write_register(uint8_t devaddr, uint8_t reg, const uint8_t *data, uint16_t length, uint16_t timeout) {
    int c = chip_of(devaddr);
    if (reg == SNLED27351_REG_COMMAND) {
        chip_page[c] = data[0];
    } else if (chip_page[c] == SNLED27351_COMMAND_PWM) {
        memcpy(&chip_pwm[c][reg], data, length);
        pwm_transfers++;
    }
    return I2C_STATUS_SUCCESS;
}

/* LED map: same shape as the K4 Pro (3 CB lines per row of 16, G line first). */
const snled27351_led_t g_snled27351_leds[RGB_MATRIX_LED_COUNT] = {
#define ROW(d, base) \
    {d, base + 0x20 + 0, base + 0, base + 0x10 + 0}, {d, base + 0x20 + 1, base + 1, base + 0x10 + 1}, \
    {d, base + 0x20 + 2, base + 2, base + 0x10 + 2}, {d, base + 0x20 + 3, base + 3, base + 0x10 + 3}, \
    {d, base + 0x20 + 4, base + 4, base + 0x10 + 4}, {d, base + 0x20 + 5, base + 5, base + 0x10 + 5}, \
    {d, base + 0x20 + 6, base + 6, base + 0x10 + 6}, {d, base + 0x20 + 7, base + 7, base + 0x10 + 7}, \
    {d, base + 0x20 + 8, base + 8, base + 0x10 + 8}, {d, base + 0x20 + 9, base + 9, base + 0x10 + 9}, \
    {d, base + 0x20 + 10, base + 10, base + 0x10 + 10}, {d, base + 0x20 + 11, base + 11, base + 0x10 + 11}, \
    {d, base + 0x20 + 12, base + 12, base + 0x10 + 12}, {d, base + 0x20 + 13, base + 13, base + 0x10 + 13}, \
    {d, base + 0x20 + 14, base + 14, base + 0x10 + 14}, {d, base + 0x20 + 15, base + 15, base + 0x10 + 15}
    ROW(0, 0x00), ROW(0, 0x30), ROW(0, 0x60), ROW(1, 0x00), ROW(1, 0x30), ROW(1, 0x60),
    {1, 0x90 + 0x20, 0x90, 0x90 + 0x10}, {1, 0x90 + 0x21, 0x91, 0x90 + 0x11}, {1, 0x90 + 0x22, 0x92, 0x90 + 0x12},
    {1, 0x90 + 0x23, 0x93, 0x90 + 0x13}, {1, 0x90 + 0x24, 0x94, 0x90 + 0x14},
#undef ROW
};

/* What the LED shows right now (chip registers, not the RAM buffer). */
static void shown(int i, uint8_t *r, uint8_t *g, uint8_t *b) {
    snled27351_led_t led = g_snled27351_leds[i];
    *r = chip_pwm[led.driver][led.r];
    *g = chip_pwm[led.driver][led.g];
    *b = chip_pwm[led.driver][led.b];
}

/* Number of LEDs showing a colour that is neither the old nor the new one. */
static int leds_mixed(uint8_t or, uint8_t og, uint8_t ob, uint8_t nr, uint8_t ng, uint8_t nb) {
    int n = 0;
    for (int i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        uint8_t r, g, b;
        shown(i, &r, &g, &b);
        bool is_old = r == or && g == og && b == ob;
        bool is_new = r == nr && g == ng && b == nb;
        if (!is_old && !is_new) n++;
    }
    return n;
}

/* One rgb_matrix frame: render everything, flush once, then let the main loop run. */
static void frame_all(uint8_t r, uint8_t g, uint8_t b) {
    snled27351_set_color_all(r, g, b);
    snled27351_flush();
}

static void main_loop_passes(int n) {
    for (int i = 0; i < n; i++) snled27351_flush_task();
}

static void test_setup(void) {
    memset(chip_page, 0, sizeof(chip_page));
    memset(chip_pwm, 0, sizeof(chip_pwm));
    snled27351_incremental_flush = true;
    snled27351_flush_chunks      = SNLED27351_FLUSH_CHUNKS_PER_STEP;
    /* Settle the RAM buffer and the chips on black. */
    snled27351_set_color_all(1, 1, 1);
    frame_all(0, 0, 0);
    main_loop_passes(100);
    pwm_transfers = 0;
}

/* ---- Tests -------------------------------------------------------------- */
TEST(backlight_on_from_black_never_shows_a_partial_frame) {
    /* Wake-up / end of a Bluetooth indication: black -> white in one frame. Written one
     * 16-byte line at a time, the green line of each row lands first: rows flash green. */
    frame_all(255, 255, 255);
    CHECK_EQ(0, leds_mixed(0, 0, 0, 255, 255, 255));
    for (int pass = 0; pass < 30; pass++) {
        main_loop_passes(1);
        CHECK_EQ(0, leds_mixed(0, 0, 0, 255, 255, 255));
    }
    uint8_t r, g, b;
    shown(50, &r, &g, &b);
    CHECK_EQ(255, r);
    CHECK_EQ(255, g);
    CHECK_EQ(255, b);
}

TEST(backlight_off_to_black_never_shows_a_partial_frame) {
    frame_all(200, 200, 200);
    main_loop_passes(100);
    frame_all(0, 0, 0); // Bluetooth indication with highlight: everything off at once
    CHECK_EQ(0, leds_mixed(200, 200, 200, 0, 0, 0));
}

TEST(small_animation_steps_stay_incremental) {
    /* A normal animation frame (small change on every LED) is still spread over the
     * main-loop passes: one 16-byte transfer per pass, not a 24-transfer block. */
    frame_all(100, 100, 100);
    main_loop_passes(100);
    pwm_transfers = 0;
    frame_all(104, 102, 98);
    CHECK_EQ(1, pwm_transfers);
    main_loop_passes(100);
    uint8_t r, g, b;
    shown(100, &r, &g, &b);
    CHECK_EQ(104, r);
    CHECK_EQ(102, g);
    CHECK_EQ(98, b);
}

TEST(a_few_keys_lighting_up_stay_incremental) {
    /* Reactive effects light a handful of keys at once: not worth a blocking flush. */
    for (int i = 0; i < 4; i++) snled27351_set_color(i * 10, 255, 255, 255);
    snled27351_flush();
    CHECK_EQ(1, pwm_transfers);
}

int main(void) {
    RUN(backlight_on_from_black_never_shows_a_partial_frame);
    RUN(backlight_off_to_black_never_shows_a_partial_frame);
    RUN(small_animation_steps_stay_incremental);
    RUN(a_few_keys_lighting_up_stay_incremental);
    return harness_report("snled27351");
}
