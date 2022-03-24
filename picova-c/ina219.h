#ifndef _INA219_H
#define _INA219_H

#include "hardware/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INA219_ADDR_DEFAULT 0x40

// An instance of an INA219 sensor. Do not access the members of this struct
// directly. Initialise it with ina219_init() and then pass it to all other
// ina219_foo() functions.
struct ina219
{
    i2c_inst_t* i2c;
    uint8_t addr;
    uint16_t cfg;
    uint16_t cal;
    float current_lsb;
    float power_lsb;
    float shunt_ohms;
};

// The result of ina219_read_data(). Do not access the members of this struct
// directly. Use the ina219_data_foo() functions to get meaningful values from
// it. This exists so that the time spent reading over I2C can be separated from
// time spent number crunching.
struct ina219_data
{
    uint16_t bus;
    uint16_t power;
    uint16_t current;
    float current_lsb;
    float power_lsb;
};

enum ina219_bus_range
{
    INA219_BUS_RANGE_16V,
    INA219_BUS_RANGE_26V,
};

enum ina219_shunt_range
{
    INA219_SHUNT_RANGE_40mV,
    INA219_SHUNT_RANGE_80mV,
    INA219_SHUNT_RANGE_160mV,
    INA219_SHUNT_RANGE_320mV,
};

enum ina219_adc
{
    INA219_ADC_BITS_9,
    INA219_ADC_BITS_10,
    INA219_ADC_BITS_11,
    INA219_ADC_BITS_12,
    INA219_ADC_SAMPLES_2,
    INA219_ADC_SAMPLES_4,
    INA219_ADC_SAMPLES_8,
    INA219_ADC_SAMPLES_16,
    INA219_ADC_SAMPLES_32,
    INA219_ADC_SAMPLES_64,
    INA219_ADC_SAMPLES_128,
};

struct ina219_cfg
{
    enum ina219_bus_range bus_range;
    enum ina219_shunt_range shunt_range;
    enum ina219_adc bus_adc;
    enum ina219_adc shunt_adc;
};

typedef struct ina219 ina219_t;
typedef struct ina219_data ina219_data_t;
typedef struct ina219_cfg ina219_cfg_t;

int ina219_init(ina219_t* hw, i2c_inst_t* i2c, uint8_t addr, float shunt_ohms);
int ina219_reset(ina219_t* hw);
int ina219_configure(ina219_t* hw, const ina219_cfg_t* cfg);
void ina219_get_config(ina219_t* hw, ina219_cfg_t* cfg);
int ina219_calibrate(ina219_t* hw);
int ina219_increase_bus_range(ina219_t* hw);
int ina219_increase_shunt_range(ina219_t* hw);

float ina219_read_shunt_mV(ina219_t* hw);
float ina219_read_bus_V(ina219_t* hw);
float ina219_read_supply_V(ina219_t* hw);
float ina219_read_power_mW(ina219_t* hw);
float ina219_read_current_mA(ina219_t* hw);

int ina219_read_data(ina219_t* hw, ina219_data_t* data);
bool ina219_data_overflowed(const ina219_data_t* data);
bool ina219_data_ready(const ina219_data_t* data);
bool ina219_data_bus_clipped(const ina219_data_t* data);
bool ina219_data_shunt_clipped(const ina219_data_t* data);
float ina219_data_bus_V(const ina219_data_t* data);
float ina219_data_power_mW(const ina219_data_t* data);
float ina219_data_current_mA(const ina219_data_t* data);

uint32_t ina219_adc_conversion_us(enum ina219_adc adc);
uint32_t ina219_cfg_conversion_us(const ina219_cfg_t* cfg);
uint32_t ina219_conversion_us(const ina219_t* hw);

#ifdef __cplusplus
}
#endif

#endif // _INA219_H
