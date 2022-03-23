#include <stdio.h>
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/multicore.h"
#include "pico/stdio_usb.h"
#include "pico/time.h"
#include "pico/util/queue.h"
#include "ina219.h"

static const uint PIN_LED = PICO_DEFAULT_LED_PIN;
static const uint PIN_SDA = 8;
static const uint PIN_SCL = 9;
static i2c_inst_t* const I2C = i2c0;
static const float SHUNT_OHMS = 0.1f;

static ina219_t ina219;
static queue_t meas_queue;

struct measurement
{
    uint32_t timestamp;
    ina219_data_t data;
};

void main_core1()
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

        queue_add_blocking(&meas_queue, &m);
        sleep_until(next);
    }
}

int main() 
{
    stdio_usb_init();

    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);
    gpio_put(PIN_LED, 1);

    i2c_init(I2C, 1000000);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SCL);
    gpio_pull_up(PIN_SDA);

    const ina219_cfg_t cfg = {
        .bus_range   = INA219_BUS_RANGE_16V,
        .shunt_range = INA219_SHUNT_RANGE_320mV,
        .bus_adc     = INA219_ADC_BITS_9,
        .shunt_adc   = INA219_ADC_BITS_11,
    };

    ina219_init(&ina219, I2C, INA219_ADDR_DEFAULT);
    ina219_configure(&ina219, &cfg);
    ina219_calibrate(&ina219, SHUNT_OHMS);

    queue_init(&meas_queue, sizeof(struct measurement), 255);
    multicore_launch_core1(main_core1);

    while (true) {
        struct measurement m;
        queue_remove_blocking(&meas_queue, &m);

        const float V = ina219_data_bus_V(&m.data);
        const float mA = ina219_data_current_mA(&m.data);
        const float mW = ina219_data_power_mW(&m.data);

        printf("%lu,%f,%f,%f\n", m.timestamp, V, mA, mW);
    }
}
