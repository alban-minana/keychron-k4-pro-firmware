/* K4 Pro board code (k4_pro.c): Cable/BT slider debounce in bluetooth_pre_task(),
 * the BT host key long press, and the battery ADC conversion. */
#include "../../keyboards/keychron/k4_pro/k4_pro.c"

#include "fakes.h"
#include "harness.h"

/* ---- Stubs of the collaborators ---------------------------------------- */
static int      connect_ex_calls, pairing_calls;
static uint8_t  pairing_host;
static uint16_t measured_voltage;

void    ckbt51_init(bool wakeup_from_low_power_mode) {}
void    ckbt51_set_local_name(const char *name) {}
void    ckbt51_set_param(module_param_t *param) {}
void    ckbt51_dfu_rx(uint8_t *data, uint8_t length) {}
void    bluetooth_init(void) {}
void    bluetooth_connect(void) {}
void    bluetooth_connect_ex(uint8_t host_idx, uint16_t timeout) { connect_ex_calls++; }
void    bluetooth_pairing_ex(uint8_t host_idx, void *param) { pairing_calls++; pairing_host = host_idx; }
void    bat_level_animiation_start(uint8_t percentage) {}
uint8_t battery_get_percentage(void) { return 50; }
void    battery_set_voltage(uint16_t value) { measured_voltage = value; }

/* The slider state lives in function-local statics of bluetooth_pre_task(), so the
 * clock is never reset between tests and the slider tests run as one sequence. */
static void test_setup(void) {
    fake_set_transport_calls = 0;
    connect_ex_calls         = 0;
    pairing_calls            = 0;
}

#define SLIDER_BT 0
#define SLIDER_USB 1

/* Hold the slider pin at `level` for `ms` milliseconds, polling every millisecond. */
static void slider_hold(uint8_t level, uint32_t ms) {
    fake_pins[USB_BT_MODE_SELECT_PIN] = level;
    for (uint32_t i = 0; i < ms; i++) {
        fake_advance(1);
        bluetooth_pre_task();
    }
}

/* ---- Slider ------------------------------------------------------------- */
TEST(slider_boot_read_is_applied_immediately) {
    fake_pins[USB_BT_MODE_SELECT_PIN] = SLIDER_BT;
    bluetooth_pre_task();
    CHECK_EQ(1, fake_set_transport_calls);
    CHECK_EQ(TRANSPORT_BLUETOOTH, fake_transport);
}

TEST(slider_stable_position_does_nothing) {
    slider_hold(SLIDER_BT, 200);
    CHECK_EQ(0, fake_set_transport_calls);
}

TEST(slider_bounce_shorter_than_debounce_is_ignored) {
    slider_hold(SLIDER_USB, MODE_SWITCH_DEBOUNCE_MS - 2);
    slider_hold(SLIDER_BT, 5);
    CHECK_EQ(0, fake_set_transport_calls);
    CHECK_EQ(TRANSPORT_BLUETOOTH, fake_transport);
}

TEST(slider_chatter_is_ignored) {
    for (int i = 0; i < 20; i++) {
        slider_hold(SLIDER_USB, 5);
        slider_hold(SLIDER_BT, 1);
    }
    CHECK_EQ(0, fake_set_transport_calls);
}

TEST(slider_stable_change_is_applied_after_debounce) {
    slider_hold(SLIDER_USB, MODE_SWITCH_DEBOUNCE_MS - 1);
    CHECK_EQ(0, fake_set_transport_calls);
    slider_hold(SLIDER_USB, 3);
    CHECK_EQ(1, fake_set_transport_calls);
    CHECK_EQ(TRANSPORT_USB, fake_transport);

    slider_hold(SLIDER_USB, 200); /* applied once */
    CHECK_EQ(1, fake_set_transport_calls);
}

TEST(slider_change_back_to_bluetooth) {
    slider_hold(SLIDER_BT, 10);
    slider_hold(SLIDER_USB, 10); /* bounce back before the debounce delay */
    CHECK_EQ(0, fake_set_transport_calls);
    slider_hold(SLIDER_BT, MODE_SWITCH_DEBOUNCE_MS + 2);
    CHECK_EQ(1, fake_set_transport_calls);
    CHECK_EQ(TRANSPORT_BLUETOOTH, fake_transport);
}

/* ---- Host keys ---------------------------------------------------------- */
static void host_key(uint16_t keycode, bool pressed) {
    keyrecord_t record = {.event.pressed = pressed};
    process_record_kb_bt(keycode, &record);
}

static void scan_for(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        fake_advance(1);
        matrix_scan_kb();
    }
}

TEST(host_key_tap_connects_without_pairing) {
    fake_transport = TRANSPORT_BLUETOOTH;
    host_key(BT_HST2, true);
    CHECK_EQ(1, connect_ex_calls);
    scan_for(500);
    host_key(BT_HST2, false);
    scan_for(PAIRING_KEY_HOLD_MS + 100);
    CHECK_EQ(0, pairing_calls);
}

TEST(host_key_long_press_starts_pairing_once) {
    fake_transport = TRANSPORT_BLUETOOTH;
    host_key(BT_HST3, true);
    scan_for(PAIRING_KEY_HOLD_MS);
    CHECK_EQ(0, pairing_calls);
    scan_for(2);
    CHECK_EQ(1, pairing_calls);
    CHECK_EQ(3, pairing_host);
    scan_for(PAIRING_KEY_HOLD_MS * 2);
    host_key(BT_HST3, false);
    CHECK_EQ(1, pairing_calls);
}

TEST(host_keys_ignored_on_usb) {
    fake_transport = TRANSPORT_USB;
    host_key(BT_HST1, true);
    scan_for(PAIRING_KEY_HOLD_MS + 100);
    CHECK_EQ(0, connect_ex_calls);
    CHECK_EQ(0, pairing_calls);
}

/* ---- Battery ADC -------------------------------------------------------- */
TEST(battery_adc_conversion) {
    battery_calculte_voltage(1781); /* ADC reading -> mV, divider ratio 2.246 */
    CHECK_EQ(4000, measured_voltage);
    battery_calculte_voltage(0);
    CHECK_EQ(0, measured_voltage);
}

int main(void) {
    fakes_reset();
    RUN(slider_boot_read_is_applied_immediately);
    RUN(slider_stable_position_does_nothing);
    RUN(slider_bounce_shorter_than_debounce_is_ignored);
    RUN(slider_chatter_is_ignored);
    RUN(slider_stable_change_is_applied_after_debounce);
    RUN(slider_change_back_to_bluetooth);
    RUN(host_key_tap_connects_without_pairing);
    RUN(host_key_long_press_starts_pairing_once);
    RUN(host_keys_ignored_on_usb);
    RUN(battery_adc_conversion);
    return harness_report("k4_pro");
}
