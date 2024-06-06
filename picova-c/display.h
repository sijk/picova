#ifndef _DISPLAY_H
#define _DISPLAY_H

#include "hardware/i2c.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

extern u8g2_t u8g2;

int display_init_i2c(i2c_inst_t *i2c, uint baudrate, uint sda_gpio, uint scl_gpio);
int display_init_ssd1306(void);

#ifdef __cplusplus
}
#endif

#endif // _DISPLAY_H
