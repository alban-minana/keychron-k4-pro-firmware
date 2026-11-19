/* Host test stub of the few QMK core declarations the code under test uses.
 * The matching fake implementations are in fakes.c. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h> /* before the dprintf macro: glibc declares dprintf() */
#include <string.h>

#include "config.h"

#define PACKED __attribute__((packed))
#define FALSE false
#define dprintf(...) \
    do {             \
    } while (0)

/* ---- GPIO --------------------------------------------------------------- */
typedef uint32_t pin_t;
#define A4 4
#define A5 5
#define A6 6
#define A8 8
#define A9 9
#define A10 10
#define B1 17
#define C14 46
#define FAKE_PIN_COUNT 64

#define PAL_MODE_INPUT 0
#define PAL_MODE_UNCONNECTED 1

uint8_t gpio_read_pin(pin_t pin);
void    gpio_write_pin(pin_t pin, uint8_t level);
void    gpio_toggle_pin(pin_t pin);
void    gpio_set_pin_input_high(pin_t pin);
void    gpio_set_pin_output_push_pull(pin_t pin);
uint8_t palReadLine(pin_t pin);
void    palSetLineMode(pin_t pin, uint32_t mode);

/* ---- Timers ------------------------------------------------------------- */
uint32_t sync_timer_read32(void);
uint32_t sync_timer_elapsed32(uint32_t last);
uint32_t timer_read32(void);
void     wait_ms(uint32_t ms);
uint32_t last_input_activity_elapsed(void);

/* ---- HID reports -------------------------------------------------------- */
#define KEYBOARD_REPORT_KEYS 6
#define NKRO_REPORT_BITS 30

enum { REPORT_ID_ALL = 0, REPORT_ID_KEYBOARD, REPORT_ID_MOUSE, REPORT_ID_SYSTEM, REPORT_ID_CONSUMER };

typedef struct {
    uint8_t mods;
    uint8_t reserved;
    uint8_t keys[KEYBOARD_REPORT_KEYS];
} PACKED report_keyboard_t;

typedef struct {
    uint8_t report_id;
    uint8_t mods;
    uint8_t bits[NKRO_REPORT_BITS];
} PACKED report_nkro_t;

typedef struct {
    uint8_t  report_id;
    uint16_t usage;
} PACKED report_extra_t;

typedef struct {
    uint8_t buttons;
    int8_t  x, y, v, h;
} PACKED report_mouse_t;

typedef struct {
    uint8_t (*keyboard_leds)(void);
    void (*send_keyboard)(report_keyboard_t *);
    void (*send_nkro)(report_nkro_t *);
    void (*send_mouse)(report_mouse_t *);
    void (*send_extra)(report_extra_t *);
    void (*send_raw_hid)(uint8_t *, uint8_t);
} host_driver_t;

/* ---- Actions / keycodes ------------------------------------------------- */
typedef struct {
    struct {
        bool pressed;
    } event;
} keyrecord_t;

typedef struct {
    bool nkro;
} keymap_config_t;
extern keymap_config_t keymap_config;

typedef union {
    uint8_t raw;
    struct {
        bool num_lock : 1;
        bool caps_lock : 1;
        bool scroll_lock : 1;
        bool compose : 1;
        bool kana : 1;
    };
} led_t;

typedef void *event_listener_t;

enum {
    KC_NO              = 0x00,
    KC_C               = 0x06,
    KC_E               = 0x08,
    KC_4               = 0x21,
    KC_TAB             = 0x2B,
    KC_SPACE           = 0x2C,
    KC_MISSION_CONTROL = 0xC1,
    KC_LAUNCHPAD       = 0xC2,
    KC_LSFT            = 0xE1,
    KC_LALT            = 0xE2,
    KC_LGUI            = 0xE3,
    KC_RALT            = 0xE6,
    KC_RGUI            = 0xE7,
    KC_TRNS            = 0x01,
    SAFE_RANGE         = 0x7E40,
};
#define KC_LWIN KC_LGUI
#define KC_LCMD KC_LGUI
#define KC_RCMD KC_RGUI
#define KC_LOPT KC_LALT
#define KC_ROPT KC_RALT
#define _______ KC_TRNS

void clear_keyboard(void);
void send_keyboard_report(void);
void register_code(uint8_t code);
void unregister_code(uint8_t code);
void default_layer_set(uint32_t state);
bool dip_switch_update_user(uint8_t index, bool active);
bool dip_switch_read(bool forced);
void keyboard_post_init_user(void);
void matrix_scan_user(void);
bool process_record_user(uint16_t keycode, keyrecord_t *record);
bool pre_process_record_user(uint16_t keycode, keyrecord_t *record);
void eeconfig_read_keymap(keymap_config_t *config);
led_t host_keyboard_led_state(void);
bool  led_update_user(led_t led_state);
void  led_update_ports(led_t led_state);
