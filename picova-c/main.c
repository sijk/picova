#include <stdio.h>
#include <string.h>
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/multicore.h"
#include "pico/stdio_usb.h"
#include "pico/time.h"
#include "pico/util/queue.h"
#include "ina219.h"
#include "u8g2.h"

static const uint PIN_LED = PICO_DEFAULT_LED_PIN;
static const uint PIN_SDA_INA219 = 12;
static const uint PIN_SCL_INA219 = 13;
static const uint PIN_VCC_INA219 = 14;
static const uint PIN_SDA_SSD1306 = 2;
static const uint PIN_SCL_SSD1306 = 3;
static const uint PIN_VCC_SSD1306 = 4;
static const uint PIN_GND_SSD1306 = 5;
static i2c_inst_t* const I2C_INA219 = i2c0;
static i2c_inst_t* const I2C_SSD1306 = i2c1;
static const float SHUNT_OHMS = 0.1f;

static ina219_t ina219;
static queue_t meas_queue;
static u8g2_t u8g2;

struct measurement
{
    uint32_t timestamp;
    ina219_data_t data;
};

static void main_core1()
{
    while (true) {
        absolute_time_t next = make_timeout_time_us(ina219_conversion_us(&ina219));

        struct measurement m;
        m.timestamp = time_us_32();

        int err = ina219_read_data(&ina219, &m.data);
        if (err < 0)
            continue;

        if (!ina219_data_ready(&m.data))
            continue;

        if (ina219_data_overflowed(&m.data)) {
            ina219_increase_shunt_range(&ina219);
            continue;
        }

        if (ina219_data_shunt_clipped(&m.data)) {
            ina219_increase_shunt_range(&ina219);
            continue;
        }

        if (ina219_data_bus_clipped(&m.data)) {
            ina219_increase_bus_range(&ina219);
            continue;
        }

        queue_add_blocking(&meas_queue, &m);
        sleep_until(next);
    }
}

static uint8_t i2c_cb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr)
{
    static uint8_t buff[32];
    static uint8_t len = 0;

    switch (msg) {
    case U8X8_MSG_BYTE_START_TRANSFER:
        len = 0;
        break;

    case U8X8_MSG_BYTE_SEND:
        memcpy(&buff[len], arg_ptr, arg_int);
        len += arg_int;
        break;

    case U8X8_MSG_BYTE_END_TRANSFER:
        int ret = i2c_write_blocking(I2C_SSD1306, u8g2_GetI2CAddress(&u8g2) >> 1, buff, len, false);
        if (ret != len)
            return 0;
        break;

    default:
        return 0;
    }

    return 1;
}

static uint8_t gpio_delay_cb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr)
{
    switch(msg) {
    case U8X8_MSG_DELAY_MILLI:
        sleep_ms(arg_int);
        break;

    default:
        return 0;
    }

    return 1;
}

int main()
{
    stdio_usb_init();

    // gpio_init(PIN_LED);
    // gpio_set_dir(PIN_LED, GPIO_OUT);
    // gpio_put(PIN_LED, 1);

    // Set up the INA219 pins
    gpio_init(PIN_VCC_INA219);
    gpio_set_dir(PIN_VCC_INA219, GPIO_OUT);
    gpio_set_drive_strength(PIN_VCC_INA219, GPIO_DRIVE_STRENGTH_12MA);
    gpio_put(PIN_VCC_INA219, 1);

    i2c_init(I2C_INA219, 1000000);
    gpio_set_function(PIN_SCL_INA219, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SDA_INA219, GPIO_FUNC_I2C);

    // Set up the SSD1306 pins
    gpio_init(PIN_GND_SSD1306);
    gpio_set_dir(PIN_GND_SSD1306, GPIO_OUT);
    gpio_set_drive_strength(PIN_GND_SSD1306, GPIO_DRIVE_STRENGTH_12MA);
    gpio_put(PIN_GND_SSD1306, 0);
    gpio_init(PIN_VCC_SSD1306);
    gpio_set_dir(PIN_VCC_SSD1306, GPIO_OUT);
    gpio_set_drive_strength(PIN_VCC_SSD1306, GPIO_DRIVE_STRENGTH_12MA);
    gpio_put(PIN_VCC_SSD1306, 1);

    i2c_init(I2C_SSD1306, 1000000);
    gpio_set_function(PIN_SCL_SSD1306, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SDA_SSD1306, GPIO_FUNC_I2C);

    // Set up the INA219
    const ina219_cfg_t cfg = {
        .bus_range   = INA219_BUS_RANGE_16V,
        .shunt_range = INA219_SHUNT_RANGE_40mV,
        .bus_adc     = INA219_ADC_BITS_9,
        .shunt_adc   = INA219_ADC_BITS_11,
    };

    ina219_init(&ina219, I2C_INA219, INA219_ADDR_DEFAULT, SHUNT_OHMS);
    ina219_configure(&ina219, &cfg);
    ina219_calibrate(&ina219);

    // Set up the SSD1306
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R2, i2c_cb, gpio_delay_cb);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont22_tr);
    u8g2_ClearBuffer(&u8g2);
    u8g2_DrawStr(&u8g2, 28, 40, "PicoVA");
    u8g2_SendBuffer(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    sleep_ms(1500);

    // Start the main loop
    queue_init(&meas_queue, sizeof(struct measurement), 255);
    multicore_launch_core1(main_core1);

    uint32_t last_update = 0;
    while (true) {
        struct measurement m;
        queue_remove_blocking(&meas_queue, &m);
        const float V = ina219_data_bus_V(&m.data);
        const float mA = ina219_data_current_mA(&m.data);
        const float mW = ina219_data_power_mW(&m.data);

        printf("%lu,%f,%f,%f\n", m.timestamp, V, mA, mW);

        if (time_us_32() - last_update >= 250000) {
            last_update = time_us_32();
            char str[11];
            u8g2_ClearBuffer(&u8g2);
            snprintf(str, sizeof(str), "%7.3f V", V);
            u8g2_DrawStr(&u8g2, 0, 18, str);
            snprintf(str, sizeof(str), "%7.3f mA", mA);
            u8g2_DrawStr(&u8g2, 0, 40, str);
            snprintf(str, sizeof(str), "%7.3f mW", mW);
            u8g2_DrawStr(&u8g2, 0, 62, str);
            u8g2_SendBuffer(&u8g2);
        }
    }
}
