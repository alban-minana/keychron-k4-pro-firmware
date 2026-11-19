/* Recording fake of the Bluetooth module transport (bluetooth_transport_t). */
#pragma once

#include "bluetooth.h"
#include "report_buffer.h"

#define FAKE_SENT_MAX 1024

typedef struct {
    uint8_t  type;  /* REPORT_TYPE_KB / NKRO / CONSUMER */
    uint16_t value; /* mods byte for KB / NKRO, usage for CONSUMER */
} fake_sent_t;

static fake_sent_t       fake_sent[FAKE_SENT_MAX];
static int               fake_sent_count;
static int               fake_connect_calls;
static bluetooth_state_t fake_bt_state;

static void fake_send_keyboard(uint8_t *report) {
    if (fake_sent_count < FAKE_SENT_MAX) fake_sent[fake_sent_count++] = (fake_sent_t){REPORT_TYPE_KB, report[0]};
}
static void fake_send_nkro(uint8_t *report) {
    if (fake_sent_count < FAKE_SENT_MAX) fake_sent[fake_sent_count++] = (fake_sent_t){REPORT_TYPE_NKRO, report[0]};
}
static void fake_send_consumer(uint16_t usage) {
    if (fake_sent_count < FAKE_SENT_MAX) fake_sent[fake_sent_count++] = (fake_sent_t){REPORT_TYPE_CONSUMER, usage};
}
static void fake_connect_ex(uint8_t host_idx, uint16_t timeout) {
    fake_connect_calls++;
}
static void fake_task(void) {}

bluetooth_transport_t bluetooth_transport = {
    .connect_ex    = fake_connect_ex,
    .send_keyboard = fake_send_keyboard,
    .send_nkro     = fake_send_nkro,
    .send_consumer = fake_send_consumer,
    .task          = fake_task,
};

static void fake_bt_transport_reset(void) {
    fake_sent_count    = 0;
    fake_connect_calls = 0;
    memset(fake_sent, 0, sizeof(fake_sent));
}

static report_buffer_t make_kb(uint8_t mods) {
    report_buffer_t r;
    memset(&r, 0, sizeof(r));
    r.type          = REPORT_TYPE_KB;
    r.keyboard.mods = mods;
    return r;
}

static report_buffer_t make_nkro(uint8_t mods) {
    report_buffer_t r;
    memset(&r, 0, sizeof(r));
    r.type      = REPORT_TYPE_NKRO;
    r.nkro.mods = mods;
    return r;
}

static report_buffer_t make_consumer(uint16_t usage) {
    report_buffer_t r;
    memset(&r, 0, sizeof(r));
    r.type     = REPORT_TYPE_CONSUMER;
    r.consumer = usage;
    return r;
}
