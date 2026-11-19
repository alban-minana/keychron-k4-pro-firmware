/* Battery: Li-ion percentage curve and battery level reporting (battery.c). */
#include "../../keyboards/keychron/bluetooth/battery.c"

#include "fakes.h"
#include "harness.h"

/* ---- Stubs of the collaborators ---------------------------------------- */
static int     reported_level_calls;
static uint8_t reported_level;
static int     shutdown_calls;

void              ckbt51_read_state_reg(uint8_t reg, uint8_t len) {}
void              ckbt51_update_bat_lvl(uint8_t percentage) { reported_level_calls++; reported_level = percentage; }
void              indicator_battery_low_enable(bool enable) {}
void              bluetooth_low_battery_shutdown(void) { shutdown_calls++; }
bluetooth_state_t bluetooth_get_state(void) { return BLUETOOTH_CONNECTED; }

static void test_setup(void) {
    fakes_reset();
    voltage               = FULL_VOLTAGE_VALUE;
    voltage_measured      = false;
    last_reported_bat_lvl = 0xFF;
    critical_low          = 0;
    bat_empty             = 0;
    reported_level_calls  = 0;
    shutdown_calls        = 0;
}

static uint8_t pct_at(uint16_t mv) {
    battery_set_voltage(mv);
    return battery_get_percentage();
}

/* ---- Tests -------------------------------------------------------------- */
TEST(percentage_at_curve_points) {
    CHECK_EQ(100, pct_at(4150));
    CHECK_EQ(90, pct_at(4050));
    CHECK_EQ(80, pct_at(3970));
    CHECK_EQ(70, pct_at(3900));
    CHECK_EQ(60, pct_at(3840));
    CHECK_EQ(50, pct_at(3800));
    CHECK_EQ(40, pct_at(3770));
    CHECK_EQ(30, pct_at(3740));
    CHECK_EQ(20, pct_at(3700));
    CHECK_EQ(10, pct_at(3620));
    CHECK_EQ(5, pct_at(3500));
    CHECK_EQ(0, pct_at(3300));
}

TEST(percentage_interpolates_between_points) {
    CHECK_EQ(95, pct_at(4100)); /* halfway 4050..4150 */
    CHECK_EQ(25, pct_at(3720)); /* halfway 3700..3740 */
    CHECK_EQ(55, pct_at(3820)); /* halfway 3800..3840 */
    CHECK_EQ(2, pct_at(3400));  /* 2.5 % rounds down */
}

TEST(percentage_is_clamped) {
    CHECK_EQ(100, pct_at(4151));
    CHECK_EQ(100, pct_at(4400));
    CHECK_EQ(100, pct_at(0xFFFF));
    CHECK_EQ(0, pct_at(3299));
    CHECK_EQ(0, pct_at(2500));
    CHECK_EQ(0, pct_at(0));
}

TEST(percentage_is_monotonic) {
    uint8_t prev = 0;
    for (uint16_t mv = 3000; mv <= 4400; mv++) {
        uint8_t p = pct_at(mv);
        if (p < prev) {
            CHECK(p >= prev);
            break;
        }
        prev = p;
    }
    CHECK_EQ(100, prev);
}

TEST(level_report_waits_for_a_measurement) {
    battery_report_level();
    CHECK_EQ(0, reported_level_calls);

    battery_set_voltage(3800);
    battery_report_level();
    CHECK_EQ(1, reported_level_calls);
    CHECK_EQ(50, reported_level);
}

TEST(level_report_hysteresis) {
    battery_set_voltage(3800); /* 50 % */
    battery_report_level();
    CHECK_EQ(1, reported_level_calls);

    battery_set_voltage(3804); /* 51 %: within hysteresis */
    battery_report_level();
    CHECK_EQ(1, reported_level_calls);

    battery_set_voltage(3808); /* 52 % */
    battery_report_level();
    CHECK_EQ(2, reported_level_calls);
    CHECK_EQ(52, reported_level);

    battery_set_voltage(3800); /* 50 %: moved by 2 downwards */
    battery_report_level();
    CHECK_EQ(3, reported_level_calls);
    CHECK_EQ(50, reported_level);

    battery_report_level_reset(); /* new connection: report again */
    battery_report_level();
    CHECK_EQ(4, reported_level_calls);
}

TEST(level_report_always_reaches_full) {
    battery_set_voltage(4140); /* 99 % */
    battery_report_level();
    battery_set_voltage(4200); /* 100 %: only +1 but always reported */
    battery_report_level();
    CHECK_EQ(2, reported_level_calls);
    CHECK_EQ(100, reported_level);
}

TEST(critical_low_shuts_down_after_consecutive_readings) {
    battery_set_voltage(SHUTDOWN_VOLTAGE_VALUE - 1);
    for (int i = 0; i < CRITICAL_LOW_COUNT; i++) battery_check_critical_low();
    CHECK_EQ(0, shutdown_calls);
    CHECK(!battery_is_critical_low());

    battery_check_critical_low();
    CHECK_EQ(1, shutdown_calls);
    CHECK(battery_is_critical_low());
}

TEST(critical_low_counter_resets_on_a_good_reading) {
    battery_set_voltage(SHUTDOWN_VOLTAGE_VALUE - 1);
    for (int i = 0; i < CRITICAL_LOW_COUNT; i++) battery_check_critical_low();
    battery_set_voltage(SHUTDOWN_VOLTAGE_VALUE);
    battery_check_critical_low();
    battery_set_voltage(SHUTDOWN_VOLTAGE_VALUE - 1);
    battery_check_critical_low();
    CHECK_EQ(0, shutdown_calls);
}

int main(void) {
    RUN(percentage_at_curve_points);
    RUN(percentage_interpolates_between_points);
    RUN(percentage_is_clamped);
    RUN(percentage_is_monotonic);
    RUN(level_report_waits_for_a_measurement);
    RUN(level_report_hysteresis);
    RUN(level_report_always_reaches_full);
    RUN(critical_low_shuts_down_after_consecutive_readings);
    RUN(critical_low_counter_resets_on_a_good_reading);
    return harness_report("battery");
}
