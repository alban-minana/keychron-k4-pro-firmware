/* Host test build: the real K4 Pro configuration, plus the values the MCU headers
 * would normally provide. Feature flags (KC_BLUETOOTH_ENABLE, ...) come from the
 * Makefile, as OPT_DEFS do in the firmware build. */
#pragma once

#define STM32_SERIAL_USE_USART2 1
#define PRODUCT "Keychron K4 Pro"
#define PRODUCT_ID 0x0240

#include "../../../keyboards/keychron/k4_pro/config.h"
