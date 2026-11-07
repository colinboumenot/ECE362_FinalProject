#ifndef I2C_STORAGE_H
#define I2C_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

void init_i2c(void);
bool save_high_score(uint16_t score);
bool load_high_score(uint16_t *score);
bool reset_high_score(void);

#endif