/* Host test stub for the LED driver suite: the fake is in test_snled27351.c. */
#pragma once
#include <stdint.h>
typedef int16_t i2c_status_t;
#define I2C_STATUS_SUCCESS 0
void         i2c_init(void);
i2c_status_t i2c_write_register(uint8_t devaddr, uint8_t regaddr, const uint8_t *data, uint16_t length, uint16_t timeout);
