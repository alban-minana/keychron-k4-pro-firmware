/* Bluetooth: report fast path vs. pending retry, and the wake-key buffer (bluetooth.c).
 * Linked with the real report_buffer.c. */
#include "../../keyboards/keychron/bluetooth/bluetooth.c"

#include "fakes.h"
#include "fake_bt_transport.h"
#include "harness.h"

/* ---- Stubs of the collaborators ---------------------------------------- */
nkro_t nkro;

void indicator_init(void) {}
void indicator_set(bluetooth_state_t state, uint8_t host_index) {}
void indicator_task(void) {}
void indicator_battery_low_enable(bool enable) {}
bool battery_is_critical_low(void) { return false; }
bool battery_is_empty(void) { return false; }
void battery_task(void) {}
void battery_report_level_reset(void) {}
void send_raw_hid(uint8_t *data, uint8_t length) {}

/* ---- Helpers ------------------------------------------------------------ */
static void test_setup(void) {
    fakes_reset();
    fake_bt_transport_reset();
    host_index   = 0;
    led_state    = 0;
    pincodeEntry = false;
    bluetooth_wake_key_buffer_clear();
    report_buffer_set_inverval(DEFAULT_REPORT_INVERVAL_MS);
    bluetooth_init(); /* state INITIALIZED, report buffer reset */
}

static void post_event(event_type_t type, uint8_t host) {
    bluetooth_event_t e = {.evt_type = type};
    e.params.hostIndex  = host;
    bluetooth_event_queue_enqueue(e);
    bluetooth_event_queue_task();
}

static void send_nkro(uint8_t mods) {
    report_nkro_t r;
    memset(&r, 0, sizeof(r));
    r.mods = mods;
    bluetooth_send_nkro(&r);
}

static void send_kb(uint8_t mods) {
    report_keyboard_t r;
    memset(&r, 0, sizeof(r));
    r.mods = mods;
    bluetooth_send_keyboard(&r);
}

/* Empty the report queue into q[] (values: mods / usage) and return how many there were. */
static uint16_t q[64];
static int      drain(void) {
    report_buffer_t r;
    int             n = 0;
    while (n < 64 && report_buffer_dequeue(&r)) {
        q[n++] = r.type == REPORT_TYPE_CONSUMER ? r.consumer : r.type == REPORT_TYPE_NKRO ? r.nkro.mods : r.keyboard.mods;
    }
    return n;
}

static void connect_idle(uint8_t host) {
    post_event(EVT_CONNECTED, host);
    fake_advance(10); /* report interval elapsed */
}

/* ---- Fast path ---------------------------------------------------------- */
TEST(nkro_fast_path_sends_directly_when_idle) {
    connect_idle(1);
    send_nkro(7);
    CHECK_EQ(1, fake_sent_count);
    CHECK_EQ(REPORT_TYPE_NKRO, fake_sent[0].type);
    CHECK_EQ(7, fake_sent[0].value);
    CHECK(report_buffer_is_empty());
    /* kb_rpt holds the report actually sent, for a later retry */
    CHECK_EQ(REPORT_TYPE_NKRO, kb_rpt.type);
    CHECK_EQ(7, kb_rpt.nkro.mods);
}

TEST(nkro_no_fast_path_while_retry_pending) {
    connect_idle(1);
    report_buffer_set_retry(5);
    send_nkro(7);
    CHECK_EQ(0, fake_sent_count);
    CHECK_EQ(1, drain());
    CHECK_EQ(7, q[0]);
}

TEST(consumer_no_fast_path_while_retry_pending) {
    connect_idle(1);
    report_buffer_set_retry(5);
    bluetooth_send_consumer(0x00E9);
    CHECK_EQ(0, fake_sent_count);
    CHECK_EQ(1, drain());
    CHECK_EQ(0x00E9, q[0]);
}

TEST(consumer_fast_path_sends_directly_when_idle) {
    connect_idle(1);
    bluetooth_send_consumer(0x00E9);
    CHECK_EQ(1, fake_sent_count);
    CHECK_EQ(REPORT_TYPE_CONSUMER, fake_sent[0].type);
    CHECK_EQ(REPORT_TYPE_CONSUMER, kb_rpt.type);
    CHECK_EQ(0x00E9, kb_rpt.consumer);
}

TEST(keyboard_no_immediate_send_while_retry_pending) {
    connect_idle(1);
    report_buffer_set_retry(5);
    send_kb(3);
    CHECK_EQ(0, fake_sent_count);
    CHECK_EQ(5, report_buffer_get_retry()); /* the pending retry is not cancelled */
    CHECK_EQ(1, drain());
}

TEST(keyboard_sent_immediately_when_idle) {
    connect_idle(1);
    send_kb(3);
    CHECK_EQ(1, fake_sent_count);
    CHECK_EQ(REPORT_TYPE_KB, fake_sent[0].type);
    CHECK_EQ(3, fake_sent[0].value);
}

TEST(no_fast_path_when_queue_not_empty) {
    connect_idle(1);
    report_buffer_t r = make_nkro(1);
    report_buffer_enqueue(&r);
    send_nkro(2);
    CHECK_EQ(0, fake_sent_count);
    CHECK_EQ(2, drain());
    CHECK_EQ(1, q[0]);
    CHECK_EQ(2, q[1]);
}

TEST(no_fast_path_within_report_interval) {
    connect_idle(1);
    send_nkro(1);
    fake_advance(1);
    send_nkro(2);
    CHECK_EQ(1, fake_sent_count);
    CHECK_EQ(1, drain());
}

TEST(retry_after_fast_path_resends_the_latest_report) {
    connect_idle(1);
    send_nkro(1);
    report_buffer_set_retry(0);
    fake_advance(10);
    send_nkro(2);
    /* The module reports "FIFO full" for report 2 and asks for a retry */
    report_buffer_set_retry(1);
    fake_advance(10);
    report_buffer_task();
    CHECK_EQ(3, fake_sent_count);
    CHECK_EQ(2, fake_sent[2].value);
}

TEST(disconnection_clears_queue_and_retry) {
    connect_idle(1);
    report_buffer_set_retry(5);
    send_nkro(1);
    post_event(EVT_DISCONNECTED, 1);
    CHECK_EQ(0, report_buffer_get_retry());
    CHECK(report_buffer_is_empty());
}

/* ---- Wake-key buffer ---------------------------------------------------- */
TEST(wake_keys_are_sent_once_connected) {
    send_nkro(1);
    send_nkro(0);
    CHECK_EQ(0, fake_sent_count);
    CHECK(fake_connect_calls > 0);
    CHECK_EQ(2, wake_key_count);

    post_event(EVT_CONNECTED, 1);
    CHECK_EQ(0, wake_key_count);

    fake_advance(10);
    report_buffer_task();
    CHECK_EQ(1, fake_sent_count);
    CHECK_EQ(1, fake_sent[0].value);

    CHECK_EQ(1, drain());
    CHECK_EQ(0, q[0]);
}

TEST(wake_keys_buffer_all_report_types) {
    send_kb(1);
    send_nkro(2);
    bluetooth_send_consumer(0x00E9);
    CHECK_EQ(3, wake_key_count);
    post_event(EVT_CONNECTED, 1);
    CHECK_EQ(3, drain());
    CHECK_EQ(1, q[0]);
    CHECK_EQ(2, q[1]);
    CHECK_EQ(0x00E9, q[2]);
}

TEST(wake_key_buffer_keeps_at_most_16_and_the_latest) {
    for (uint8_t i = 1; i <= 20; i++) send_nkro(i);
    CHECK_EQ(16, BT_WAKE_KEY_BUFFER_SIZE);
    CHECK_EQ(BT_WAKE_KEY_BUFFER_SIZE, wake_key_count);

    post_event(EVT_CONNECTED, 1);
    CHECK_EQ(BT_WAKE_KEY_BUFFER_SIZE, drain());
    for (int i = 0; i < BT_WAKE_KEY_BUFFER_SIZE - 1; i++) CHECK_EQ(i + 1, q[i]);
    /* The last slot holds the final state */
    CHECK_EQ(20, q[BT_WAKE_KEY_BUFFER_SIZE - 1]);
}

TEST(wake_keys_kept_up_to_the_timeout) {
    send_nkro(1);
    fake_advance(BT_WAKE_KEY_BUFFER_MS);
    post_event(EVT_CONNECTED, 1);
    CHECK_EQ(1, drain());
}

TEST(wake_keys_dropped_after_the_timeout) {
    send_nkro(1);
    fake_advance(BT_WAKE_KEY_BUFFER_MS + 1);
    post_event(EVT_CONNECTED, 1);
    CHECK_EQ(0, drain());
    CHECK_EQ(0, wake_key_count);
}

TEST(wake_key_timeout_counts_from_the_first_key) {
    send_nkro(1);
    fake_advance(BT_WAKE_KEY_BUFFER_MS - 100);
    send_nkro(0);
    fake_advance(200);
    post_event(EVT_CONNECTED, 1);
    CHECK_EQ(0, drain());
}

TEST(expired_wake_keys_are_replaced_by_new_ones) {
    send_nkro(1);
    fake_advance(BT_WAKE_KEY_BUFFER_MS + 1);
    send_nkro(2);
    CHECK_EQ(1, wake_key_count);
    post_event(EVT_CONNECTED, 1);
    CHECK_EQ(1, drain());
    CHECK_EQ(2, q[0]);
}

TEST(wake_keys_kept_when_reconnecting_to_the_same_host) {
    post_event(EVT_CONNECTED, 1);
    post_event(EVT_DISCONNECTED, 1);
    send_nkro(1);
    bluetooth_connect_ex(1, 0);
    CHECK_EQ(1, wake_key_count);
}

TEST(wake_keys_cleared_on_host_change) {
    post_event(EVT_CONNECTED, 1);
    post_event(EVT_DISCONNECTED, 1);
    send_nkro(1);
    bluetooth_connect_ex(2, 0);
    CHECK_EQ(0, wake_key_count);
    post_event(EVT_CONNECTED, 2);
    CHECK_EQ(0, drain());
}

TEST(nothing_buffered_in_reset_state) {
    bt_state = BLUETOOTH_RESET;
    send_nkro(1);
    CHECK_EQ(0, wake_key_count);
    CHECK_EQ(0, fake_connect_calls);
}

int main(void) {
    RUN(nkro_fast_path_sends_directly_when_idle);
    RUN(nkro_no_fast_path_while_retry_pending);
    RUN(consumer_no_fast_path_while_retry_pending);
    RUN(consumer_fast_path_sends_directly_when_idle);
    RUN(keyboard_no_immediate_send_while_retry_pending);
    RUN(keyboard_sent_immediately_when_idle);
    RUN(no_fast_path_when_queue_not_empty);
    RUN(no_fast_path_within_report_interval);
    RUN(retry_after_fast_path_resends_the_latest_report);
    RUN(disconnection_clears_queue_and_retry);
    RUN(wake_keys_are_sent_once_connected);
    RUN(wake_keys_buffer_all_report_types);
    RUN(wake_key_buffer_keeps_at_most_16_and_the_latest);
    RUN(wake_keys_kept_up_to_the_timeout);
    RUN(wake_keys_dropped_after_the_timeout);
    RUN(wake_key_timeout_counts_from_the_first_key);
    RUN(expired_wake_keys_are_replaced_by_new_ones);
    RUN(wake_keys_kept_when_reconnecting_to_the_same_host);
    RUN(wake_keys_cleared_on_host_change);
    RUN(nothing_buffered_in_reset_state);
    return harness_report("bluetooth");
}
