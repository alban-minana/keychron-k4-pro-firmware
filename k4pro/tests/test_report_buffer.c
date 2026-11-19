/* Report buffer: FIFO queue and the send / retry task (report_buffer.c). */
#include "../../keyboards/keychron/bluetooth/report_buffer.c"

#include "fakes.h"
#include "fake_bt_transport.h"
#include "harness.h"

bluetooth_state_t bluetooth_get_state(void) {
    return fake_bt_state;
}

static void test_setup(void) {
    fakes_reset();
    fake_bt_transport_reset();
    fake_bt_state = BLUETOOTH_CONNECTED;
    report_buffer_set_inverval(DEFAULT_REPORT_INVERVAL_MS);
    report_buffer_init();
}

static void enqueue_kb(uint8_t mods) {
    report_buffer_t r = make_kb(mods);
    report_buffer_enqueue(&r);
}

/* Advance the clock and run the task once. */
static void tick(uint32_t ms) {
    fake_advance(ms);
    report_buffer_task();
}

/* ---- Queue -------------------------------------------------------------- */
TEST(queue_starts_empty) {
    report_buffer_t r;
    CHECK(report_buffer_is_empty());
    CHECK(!report_buffer_dequeue(&r));
}

TEST(queue_is_fifo) {
    enqueue_kb(1);
    enqueue_kb(2);
    enqueue_kb(3);
    CHECK(!report_buffer_is_empty());

    report_buffer_t r;
    CHECK(report_buffer_dequeue(&r));
    CHECK_EQ(1, r.keyboard.mods);
    CHECK(report_buffer_dequeue(&r));
    CHECK_EQ(2, r.keyboard.mods);
    CHECK(report_buffer_dequeue(&r));
    CHECK_EQ(3, r.keyboard.mods);
    CHECK(!report_buffer_dequeue(&r));
    CHECK(report_buffer_is_empty());
}

TEST(queue_full_rejects_and_keeps_content) {
    int accepted = 0;
    for (int i = 0; i < REPORT_BUFFER_QUEUE_SIZE + 10; i++) {
        report_buffer_t r = make_kb((uint8_t)i);
        if (report_buffer_enqueue(&r)) accepted++;
    }
    /* One slot is kept free to tell "full" from "empty" */
    CHECK_EQ(REPORT_BUFFER_QUEUE_SIZE - 1, accepted);

    report_buffer_t r = make_kb(0xAA);
    CHECK(!report_buffer_enqueue(&r));

    /* The oldest report is still first; dequeuing one makes room again */
    CHECK(report_buffer_dequeue(&r));
    CHECK_EQ(0, r.keyboard.mods);
    r = make_kb(0xAA);
    CHECK(report_buffer_enqueue(&r));

    for (int i = 1; i < REPORT_BUFFER_QUEUE_SIZE - 1; i++) {
        CHECK(report_buffer_dequeue(&r));
        if (r.keyboard.mods != (uint8_t)i) {
            CHECK_EQ((uint8_t)i, r.keyboard.mods);
            break;
        }
    }
    CHECK(report_buffer_dequeue(&r));
    CHECK_EQ(0xAA, r.keyboard.mods);
    CHECK(report_buffer_is_empty());
}

TEST(queue_wraps_around) {
    report_buffer_t r;
    for (int i = 0; i < 3 * REPORT_BUFFER_QUEUE_SIZE; i++) {
        enqueue_kb((uint8_t)i);
        CHECK(report_buffer_dequeue(&r));
        if (r.keyboard.mods != (uint8_t)i) {
            CHECK_EQ((uint8_t)i, r.keyboard.mods);
            break;
        }
    }
    CHECK(report_buffer_is_empty());
}

/* ---- Task --------------------------------------------------------------- */
TEST(task_does_nothing_when_not_connected) {
    fake_bt_state = BLUETOOTH_DISCONNECTED;
    enqueue_kb(1);
    tick(100);
    CHECK_EQ(0, fake_sent_count);
    CHECK(!report_buffer_is_empty());
}

TEST(task_waits_for_the_report_interval) {
    enqueue_kb(1);
    tick(DEFAULT_REPORT_INVERVAL_MS); /* elapsed must be strictly greater */
    CHECK_EQ(0, fake_sent_count);
    tick(1);
    CHECK_EQ(1, fake_sent_count);
    CHECK_EQ(REPORT_TYPE_KB, fake_sent[0].type);
    CHECK_EQ(1, fake_sent[0].value);
}

TEST(task_sends_each_report_type_with_its_function) {
    report_buffer_t r = make_nkro(5);
    report_buffer_enqueue(&r);
    r = make_consumer(0x00E9);
    report_buffer_enqueue(&r);

    tick(10);
    report_buffer_set_retry(0); /* acknowledged by the module */
    tick(10);

    CHECK_EQ(2, fake_sent_count);
    CHECK_EQ(REPORT_TYPE_NKRO, fake_sent[0].type);
    CHECK_EQ(5, fake_sent[0].value);
    CHECK_EQ(REPORT_TYPE_CONSUMER, fake_sent[1].type);
    CHECK_EQ(0x00E9, fake_sent[1].value);
}

TEST(task_skips_empty_report_slots) {
    report_buffer_t r;
    memset(&r, 0, sizeof(r)); /* REPORT_TYPE_NONE */
    report_buffer_enqueue(&r);
    tick(10);
    CHECK_EQ(0, fake_sent_count);
    CHECK_EQ(0, report_buffer_get_retry());
    CHECK(report_buffer_is_empty());
}

TEST(sent_report_arms_the_retry) {
    enqueue_kb(1);
    tick(10);
    CHECK_EQ(1, fake_sent_count);
    CHECK_EQ(RETPORT_RETRY_COUNT, report_buffer_get_retry());
}

TEST(retry_resends_after_7ms_until_acknowledged) {
    enqueue_kb(1);
    enqueue_kb(2);
    tick(10); /* sends 1 */
    CHECK_EQ(1, fake_sent_count);

    tick(4); /* report interval elapsed, retry delay (> 7 ms) not yet */
    CHECK_EQ(1, fake_sent_count);
    tick(3); /* 7 ms since the send: still not > 7 */
    CHECK_EQ(1, fake_sent_count);
    tick(1); /* 8 ms: resend the same report */
    CHECK_EQ(2, fake_sent_count);
    CHECK_EQ(1, fake_sent[1].value);
    CHECK_EQ(RETPORT_RETRY_COUNT - 1, report_buffer_get_retry());

    /* The module acknowledges: the next report goes out */
    report_buffer_set_retry(0);
    tick(10);
    CHECK_EQ(3, fake_sent_count);
    CHECK_EQ(2, fake_sent[2].value);
}

TEST(retry_counts_down_then_moves_on) {
    enqueue_kb(1);
    enqueue_kb(2);
    tick(10);
    for (int i = 0; i < RETPORT_RETRY_COUNT; i++) tick(10);

    /* 1 send + RETPORT_RETRY_COUNT resends, all of report 1 */
    CHECK_EQ(1 + RETPORT_RETRY_COUNT, fake_sent_count);
    for (int i = 0; i < fake_sent_count; i++) {
        if (fake_sent[i].value != 1) {
            CHECK_EQ(1, fake_sent[i].value);
            break;
        }
    }
    CHECK_EQ(0, report_buffer_get_retry());

    tick(10);
    CHECK_EQ(2 + RETPORT_RETRY_COUNT, fake_sent_count);
    CHECK_EQ(2, fake_sent[fake_sent_count - 1].value);
}

TEST(retry_keeps_running_with_an_empty_queue) {
    enqueue_kb(1);
    tick(10);
    CHECK(report_buffer_is_empty());
    tick(10);
    CHECK_EQ(2, fake_sent_count);
}

TEST(init_clears_queue_and_retry) {
    enqueue_kb(1);
    enqueue_kb(2);
    tick(10);
    report_buffer_init();
    CHECK(report_buffer_is_empty());
    CHECK_EQ(0, report_buffer_get_retry());
    tick(10);
    CHECK_EQ(1, fake_sent_count);
}

int main(void) {
    RUN(queue_starts_empty);
    RUN(queue_is_fifo);
    RUN(queue_full_rejects_and_keeps_content);
    RUN(queue_wraps_around);
    RUN(task_does_nothing_when_not_connected);
    RUN(task_waits_for_the_report_interval);
    RUN(task_sends_each_report_type_with_its_function);
    RUN(task_skips_empty_report_slots);
    RUN(sent_report_arms_the_retry);
    RUN(retry_resends_after_7ms_until_acknowledged);
    RUN(retry_counts_down_then_moves_on);
    RUN(retry_keeps_running_with_an_empty_queue);
    RUN(init_clears_queue_and_retry);
    return harness_report("report_buffer");
}
