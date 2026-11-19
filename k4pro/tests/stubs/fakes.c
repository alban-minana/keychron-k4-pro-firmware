/* Fake implementations of the services the modules under test depend on but which are
 * never tested themselves (system/RTC timers, GPIO, transport, LPM, QMK core). */
#include "fakes.h"
#include "lpm.h"
#include "rtc_timer.h"

uint32_t    fake_time_ms;
uint8_t     fake_pins[FAKE_PIN_COUNT];
transport_t fake_transport;
int         fake_set_transport_calls;
uint32_t    fake_input_elapsed_ms;
bool        fake_usb_power;

keymap_config_t keymap_config;

void fakes_reset(void) {
    fake_time_ms = 1000; /* some code treats a zero timestamp as "not started" */
    memset(fake_pins, 0, sizeof(fake_pins));
    fake_transport           = TRANSPORT_BLUETOOTH;
    fake_set_transport_calls = 0;
    fake_input_elapsed_ms    = 0;
    fake_usb_power           = false;
    memset(&keymap_config, 0, sizeof(keymap_config));
}

void fake_advance(uint32_t ms) {
    fake_time_ms += ms;
}

/* Timers */
uint32_t sync_timer_read32(void) { return fake_time_ms; }
uint32_t sync_timer_elapsed32(uint32_t last) { return fake_time_ms - last; }
uint32_t timer_read32(void) { return fake_time_ms; }
void     wait_ms(uint32_t ms) { fake_time_ms += ms; }
uint32_t last_input_activity_elapsed(void) { return fake_input_elapsed_ms; }

void     rtc_timer_init(void) {}
void     rtc_timer_clear(void) {}
uint32_t rtc_timer_read_ms(void) { return fake_time_ms; }
uint32_t rtc_timer_elapsed_ms(uint32_t last) { return fake_time_ms - last; }

/* GPIO */
uint8_t gpio_read_pin(pin_t pin) { return fake_pins[pin]; }
void    gpio_write_pin(pin_t pin, uint8_t level) { fake_pins[pin] = level; }
void    gpio_toggle_pin(pin_t pin) { fake_pins[pin] = !fake_pins[pin]; }
void    gpio_set_pin_input_high(pin_t pin) {}
void    gpio_set_pin_output_push_pull(pin_t pin) {}
uint8_t palReadLine(pin_t pin) { return fake_pins[pin]; }
void    palSetLineMode(pin_t pin, uint32_t mode) {}

/* Transport */
void set_transport(transport_t new_transport) {
    fake_transport = new_transport;
    fake_set_transport_calls++;
}
transport_t get_transport(void) { return fake_transport; }

/* Low power management */
void lpm_init(void) {}
void lpm_timer_reset(void) {}
void lpm_task(void) {}
bool usb_power_connected(void) { return fake_usb_power; }

/* QMK core */
void  clear_keyboard(void) {}
void  send_keyboard_report(void) {}
void  register_code(uint8_t code) {}
void  unregister_code(uint8_t code) {}
void  default_layer_set(uint32_t state) {}
bool  dip_switch_update_user(uint8_t index, bool active) { return true; }
bool  dip_switch_read(bool forced) { return true; }
void  keyboard_post_init_user(void) {}
void  matrix_scan_user(void) {}
bool  process_record_user(uint16_t keycode, keyrecord_t *record) { return true; }
bool  pre_process_record_user(uint16_t keycode, keyrecord_t *record) { return true; }
void  eeconfig_read_keymap(keymap_config_t *config) {}
led_t host_keyboard_led_state(void) { return (led_t){0}; }
bool  led_update_user(led_t led_state) { return true; }
void  led_update_ports(led_t led_state) {}
