/* Controllable fakes for the hardware / QMK core services used by the code under test. */
#pragma once

#include "quantum.h"
#include "transport.h"

extern uint32_t    fake_time_ms;           /* returned by sync_timer_read32() and rtc_timer_read_ms() */
extern uint8_t     fake_pins[FAKE_PIN_COUNT];
extern transport_t fake_transport;          /* returned by get_transport() */
extern int         fake_set_transport_calls;
extern uint32_t    fake_input_elapsed_ms;   /* returned by last_input_activity_elapsed() */
extern bool        fake_usb_power;          /* returned by usb_power_connected() */

void fakes_reset(void);
void fake_advance(uint32_t ms);
