/* Host test stub for the LED driver suite. */
#pragma once
#include <stdint.h>
typedef uint32_t pin_t;
static inline void gpio_set_pin_output(pin_t pin) {}
static inline void gpio_write_pin_high(pin_t pin) {}
static inline void gpio_write_pin_low(pin_t pin) {}
