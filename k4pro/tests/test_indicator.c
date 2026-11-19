/* Indicator: backlight dimming before the battery timeout (indicator_backlit_dim_task
 * in indicator.c), built with RGB_MATRIX_ENABLE like the RGB variants. */
#include "../../keyboards/keychron/bluetooth/indicator.c"

#include "fakes.h"
#include "harness.h"

/* ---- Stubs of the collaborators ---------------------------------------- */
rgb_config_t rgb_matrix_config;
static bool  fake_rgb_enabled;
static void  fake_flush(void) {}
const rgb_matrix_driver_t rgb_matrix_driver = {.flush = fake_flush};

bool rgb_matrix_is_enabled(void) { return fake_rgb_enabled; }
void rgb_matrix_enable_noeeprom(void) { fake_rgb_enabled = true; }
void rgb_matrix_disable_noeeprom(void) { fake_rgb_enabled = false; }
void rgb_matrix_disable_timeout_set(uint32_t timeout) {}
void rgb_matrix_disable_time_reset(void) {}
void rgb_matrix_set_color(int index, uint8_t r, uint8_t g, uint8_t b) {}
void rgb_matrix_set_color_all(uint8_t r, uint8_t g, uint8_t b) {}
bool rgb_matrix_indicators_user(void) { return true; }
void eeconfig_read_rgb_matrix(rgb_config_t *config) {}
void eeconfig_update_rgb_matrix_default(void) {}

void bat_level_animiation_start(uint8_t percentage) {}
void bat_level_animiation_stop(void) {}
bool bat_level_animiation_actived(void) { return false; }
void bat_level_animiation_indicate(void) {}
void bat_level_animiation_task(void) {}
bool battery_is_critical_low(void) { return false; }
bool battery_is_empty(void) { return false; }

/* ---- Helpers ------------------------------------------------------------ */
#define TIMEOUT_MS 180000u /* CONNECTED_BACKLIGHT_DISABLE_TIMEOUT of the K4 Pro */
#define DIM_START (TIMEOUT_MS - BACKLIGHT_DIM_LEAD_MS)

static void test_setup(void) {
    fakes_reset();
    memset(&indicator_config, 0, sizeof(indicator_config));
    dim_active              = false;
    fake_rgb_enabled        = true;
    rgb_matrix_config.hsv.v = 200;
    indicator_set_backlit_timeout(TIMEOUT_MS);
}

/* Run the dim task with the given idle time and return the resulting brightness. */
static uint8_t val_after_idle(uint32_t idle_ms) {
    fake_input_elapsed_ms = idle_ms;
    indicator_backlit_dim_task();
    return rgb_matrix_config.hsv.v;
}

static void key_press(void) {
    keyrecord_t record = {.event.pressed = true};
    fake_input_elapsed_ms = 0;
    pre_process_record_kb(0, &record);
}

/* ---- Tests -------------------------------------------------------------- */
TEST(no_dim_before_timeout_minus_lead) {
    CHECK_EQ(200, val_after_idle(0));
    CHECK_EQ(200, val_after_idle(DIM_START - 1));
    CHECK(!dim_active);
}

TEST(fades_to_30_percent_over_1s) {
    CHECK_EQ(200, val_after_idle(DIM_START));
    CHECK(dim_active);
    CHECK_EQ(165, val_after_idle(DIM_START + 250));  /* 200 - 140 * 0.25 */
    CHECK_EQ(130, val_after_idle(DIM_START + 500));
    CHECK_EQ(95, val_after_idle(DIM_START + 750));
    CHECK_EQ(60, val_after_idle(DIM_START + BACKLIGHT_DIM_FADE_MS));
    CHECK_EQ(60, val_after_idle(DIM_START + 5000));
    CHECK_EQ(60, val_after_idle(TIMEOUT_MS - 1));
}

TEST(fade_is_monotonic) {
    uint8_t prev = 255;
    for (uint32_t t = DIM_START; t <= DIM_START + BACKLIGHT_DIM_FADE_MS + 10; t++) {
        uint8_t v = val_after_idle(t);
        if (v > prev) {
            CHECK(v <= prev);
            break;
        }
        prev = v;
    }
    CHECK_EQ(60, prev);
}

TEST(untouched_once_backlight_is_off) {
    val_after_idle(DIM_START + BACKLIGHT_DIM_FADE_MS);
    CHECK_EQ(60, val_after_idle(TIMEOUT_MS));
    CHECK_EQ(60, val_after_idle(TIMEOUT_MS + 60000));
    CHECK(dim_active);
}

TEST(key_press_restores_brightness) {
    val_after_idle(DIM_START + 500);
    key_press();
    CHECK_EQ(200, rgb_matrix_config.hsv.v);
    CHECK(!dim_active);
}

TEST(activity_restores_brightness) {
    val_after_idle(DIM_START + 500);
    CHECK_EQ(200, val_after_idle(10));
    CHECK(!dim_active);
}

TEST(restore_after_backlight_off) {
    val_after_idle(DIM_START + BACKLIGHT_DIM_FADE_MS);
    val_after_idle(TIMEOUT_MS + 1000);
    key_press();
    CHECK_EQ(200, rgb_matrix_config.hsv.v);
}

TEST(restore_keeps_a_brightness_changed_meanwhile) {
    val_after_idle(DIM_START + 500);
    rgb_matrix_config.hsv.v = 100; /* VIA / EEPROM reload */
    key_press();
    CHECK_EQ(100, rgb_matrix_config.hsv.v);
}

TEST(dimming_stops_if_brightness_changed_meanwhile) {
    val_after_idle(DIM_START + 250);
    rgb_matrix_config.hsv.v = 100;
    CHECK_EQ(100, val_after_idle(DIM_START + 500));
    CHECK(!dim_active);
}

TEST(dim_of_zero_brightness_stays_zero) {
    rgb_matrix_config.hsv.v = 0;
    CHECK_EQ(0, val_after_idle(DIM_START + 500));
    key_press();
    CHECK_EQ(0, rgb_matrix_config.hsv.v);
}

TEST(no_dim_for_short_timeouts) {
    /* Below 2 * lead (20 s) there is no dimming at all */
    indicator_set_backlit_timeout(2 * BACKLIGHT_DIM_LEAD_MS - 1);
    CHECK_EQ(200, val_after_idle(2 * BACKLIGHT_DIM_LEAD_MS - 2));
    CHECK(!dim_active);

    indicator_set_backlit_timeout(1000); /* critical battery */
    CHECK_EQ(200, val_after_idle(999));

    indicator_set_backlit_timeout(2 * BACKLIGHT_DIM_LEAD_MS); /* 20 s: dims from 10 s */
    CHECK_EQ(200, val_after_idle(BACKLIGHT_DIM_LEAD_MS - 1));
    CHECK_EQ(60, val_after_idle(BACKLIGHT_DIM_LEAD_MS + BACKLIGHT_DIM_FADE_MS));
}

TEST(no_dim_for_infinite_timeout) {
    indicator_set_backlit_timeout(RGB_MATRIX_TIMEOUT_INFINITE);
    CHECK_EQ(200, val_after_idle(UINT32_MAX - 1));
    CHECK_EQ(200, val_after_idle(TIMEOUT_MS));
    CHECK(!dim_active);
}

TEST(no_dim_on_usb) {
    fake_transport = TRANSPORT_USB;
    CHECK_EQ(200, val_after_idle(DIM_START + 500));
    CHECK(!dim_active);
}

TEST(switching_to_usb_while_dimmed_restores) {
    val_after_idle(DIM_START + 500);
    fake_transport = TRANSPORT_USB;
    CHECK_EQ(200, val_after_idle(DIM_START + 600));
}

TEST(no_dim_when_backlight_disabled) {
    fake_rgb_enabled = false;
    CHECK_EQ(200, val_after_idle(DIM_START + 500));
    CHECK(!dim_active);
}

TEST(dim_runs_from_indicator_task) {
    fake_input_elapsed_ms = DIM_START + BACKLIGHT_DIM_FADE_MS;
    indicator_task();
    CHECK_EQ(60, rgb_matrix_config.hsv.v);
}

int main(void) {
    RUN(no_dim_before_timeout_minus_lead);
    RUN(fades_to_30_percent_over_1s);
    RUN(fade_is_monotonic);
    RUN(untouched_once_backlight_is_off);
    RUN(key_press_restores_brightness);
    RUN(activity_restores_brightness);
    RUN(restore_after_backlight_off);
    RUN(restore_keeps_a_brightness_changed_meanwhile);
    RUN(dimming_stops_if_brightness_changed_meanwhile);
    RUN(dim_of_zero_brightness_stays_zero);
    RUN(no_dim_for_short_timeouts);
    RUN(no_dim_for_infinite_timeout);
    RUN(no_dim_on_usb);
    RUN(switching_to_usb_while_dimmed_restores);
    RUN(no_dim_when_backlight_disabled);
    RUN(dim_runs_from_indicator_task);
    return harness_report("indicator");
}
