/* Host test stub of the RGB Matrix API used by indicator.c. */
#pragma once

#include "quantum.h"

#define RGB_MATRIX_TIMEOUT_INFINITE (UINT32_MAX)

typedef struct {
    uint8_t mode;
    struct {
        uint8_t h, s, v;
    } hsv;
} rgb_config_t;

typedef struct {
    void (*flush)(void);
} rgb_matrix_driver_t;

extern rgb_config_t              rgb_matrix_config;
extern const rgb_matrix_driver_t rgb_matrix_driver;

bool rgb_matrix_is_enabled(void);
void rgb_matrix_enable_noeeprom(void);
void rgb_matrix_disable_noeeprom(void);
void rgb_matrix_disable_timeout_set(uint32_t timeout);
void rgb_matrix_disable_time_reset(void);
void rgb_matrix_set_color(int index, uint8_t r, uint8_t g, uint8_t b);
void rgb_matrix_set_color_all(uint8_t r, uint8_t g, uint8_t b);
bool rgb_matrix_indicators_user(void);
bool rgb_matrix_indicators_kb(void);
void rgb_matrix_none_indicators_kb(void);
void eeconfig_read_rgb_matrix(rgb_config_t *config);
void eeconfig_update_rgb_matrix_default(void);
