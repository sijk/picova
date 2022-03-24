#include <math.h>
#include "ina219.h"

enum ina219_reg
{
    INA219_REG_CFG,
    INA219_REG_SHUNT,
    INA219_REG_BUS,
    INA219_REG_POWER,
    INA219_REG_CURRENT,
    INA219_REG_CALIB,
};

static const uint16_t INA219_BUS_OVF  = (1 << 0);
static const uint16_t INA219_BUS_CNVR = (1 << 1);

static int ina219_read_reg(ina219_t* hw, uint8_t reg, uint16_t* value)
{
    int err;
    uint8_t buff[2];

    err = i2c_write_blocking(hw->i2c, hw->addr, &reg, sizeof(reg), true);
    if (err < 0) 
        return err;

    err = i2c_read_blocking(hw->i2c, hw->addr, buff, sizeof(buff), false);
    if (err < 0) 
        return err;

    *value = (buff[0] << 8) | buff[1];
    return PICO_OK;
}

static int ina219_write_reg(ina219_t* hw, uint8_t reg, uint16_t value)
{
    const uint8_t buff[3] = {
        reg,
        value >> 8,
        value & 0xFF
    };

    return i2c_write_blocking(hw->i2c, hw->addr, buff, sizeof(buff), false);
}

int ina219_init(ina219_t* hw, i2c_inst_t* i2c, uint8_t addr, float shunt_ohms)
{
    hw->i2c = i2c;
    hw->addr = addr;
    hw->cfg = 0x399F;
    hw->cal = 0;
    hw->current_lsb = 0.f;
    hw->power_lsb = 0.f;
    hw->shunt_ohms = shunt_ohms;
}

int ina219_reset(ina219_t* hw)
{
    return ina219_write_reg(hw, INA219_REG_CFG, 0x8000);
}

int ina219_configure(ina219_t* hw, const ina219_cfg_t* cfg)
{
    uint16_t reg = 0;

    if (cfg->bus_range == INA219_BUS_RANGE_26V)
        reg |= (1 << 13);

    reg |= (cfg->shunt_range << 11);

    const uint8_t bus_adc = cfg->bus_adc > INA219_ADC_BITS_12
        ? cfg->bus_adc + 5
        : cfg->bus_adc;

    const uint8_t shunt_adc = cfg->shunt_adc > INA219_ADC_BITS_12
        ? cfg->shunt_adc + 5
        : cfg->shunt_adc;

    reg |= (bus_adc << 7);
    reg |= (shunt_adc << 3);

    reg |= 0x07; // Mode = shunt and bus, continuous

    hw->cfg = reg;
    return ina219_write_reg(hw, INA219_REG_CFG, reg);
}

static void ina219_calc_config(uint16_t reg, ina219_cfg_t* cfg)
{
    cfg->bus_range = (reg >> 13) & 0x01;
    cfg->shunt_range = (reg >> 11) & 0x03;
    cfg->bus_adc = (reg >> 7) & 0x0F;
    cfg->shunt_adc = (reg >> 3) & 0x0F;
}

void ina219_get_config(ina219_t* hw, ina219_cfg_t* cfg)
{
    ina219_calc_config(hw->cfg, cfg);
}

static float ina219_calc_max_shunt_V(uint16_t reg)
{
    enum ina219_shunt_range range = (reg >> 11) & 0x03;

    switch (range) {
    case INA219_SHUNT_RANGE_40mV:   return 0.04f;
    case INA219_SHUNT_RANGE_80mV:   return 0.08f;
    case INA219_SHUNT_RANGE_160mV:  return 0.16f;
    case INA219_SHUNT_RANGE_320mV:  return 0.32f;
    }

    return NAN;
}

int ina219_calibrate(ina219_t* hw)
{
    const float max_current_A = ina219_calc_max_shunt_V(hw->cfg) / hw->shunt_ohms;
    const float current_lsb = max_current_A / (1 << 15);
    const float min_lsb = 0.04096f / (0xFFFE * hw->shunt_ohms);

    hw->current_lsb = fmax(current_lsb, min_lsb);
    hw->cal = 0.04096f / (hw->current_lsb * hw->shunt_ohms);
    hw->power_lsb = 20 * hw->current_lsb;

    return ina219_write_reg(hw, INA219_REG_CALIB, hw->cal);
}

int ina219_increase_bus_range(ina219_t* hw)
{
    int err;
    ina219_cfg_t cfg;
    ina219_calc_config(hw->cfg, &cfg);

    if (cfg.bus_range < INA219_BUS_RANGE_26V) {
        cfg.bus_range++;

        err = ina219_configure(hw, &cfg);
        if (err < 0)
            return err;
    }

    return PICO_OK;
}

int ina219_increase_shunt_range(ina219_t* hw)
{
    int err;
    ina219_cfg_t cfg;
    ina219_calc_config(hw->cfg, &cfg);

    if (cfg.shunt_range < INA219_SHUNT_RANGE_320mV) {
        cfg.shunt_range++;

        err = ina219_configure(hw, &cfg);
        if (err < 0)
            return err;

        err = ina219_calibrate(hw);
        if (err < 0)
            return err;
    }

    return PICO_OK;
}

static float ina219_calc_shunt_mV(uint16_t reg)
{
    return ((int16_t)reg) * 10e-3f;
}

static float ina219_calc_bus_V(uint16_t reg)
{
    if (reg & INA219_BUS_OVF)
        return INFINITY;

    if (!(reg & INA219_BUS_CNVR))
        return NAN;

    return (reg >> 3) * 4e-3f;
}

static float ina219_calc_power_mW(uint16_t reg, float lsb)
{
    return reg * lsb * 1000.f;
}

static float ina219_calc_current_mA(uint16_t reg, float lsb)
{
    return (int16_t)reg * lsb * 1000.f;
}

float ina219_read_shunt_mV(ina219_t* hw)
{
    uint16_t reg;
    int err = ina219_read_reg(hw, INA219_REG_SHUNT, &reg);
    if (err < 0)
        return NAN;

    return ina219_calc_shunt_mV(reg);
}

float ina219_read_bus_V(ina219_t* hw)
{
    uint16_t reg;
    int err = ina219_read_reg(hw, INA219_REG_BUS, &reg);
    if (err < 0)
        return NAN;

    return ina219_calc_bus_V(reg);
}

float ina219_read_supply_V(ina219_t* hw)
{
    return ina219_read_bus_V(hw) + ina219_read_shunt_mV(hw) * 1000;
}

float ina219_read_power_mW(ina219_t* hw)
{
    uint16_t reg;
    int err = ina219_read_reg(hw, INA219_REG_POWER, &reg);
    if (err < 0)
        return NAN;

    return ina219_calc_power_mW(reg, hw->power_lsb);
}

float ina219_read_current_mA(ina219_t* hw)
{
    uint16_t reg;
    int err = ina219_read_reg(hw, INA219_REG_CURRENT, &reg);
    if (err < 0)
        return NAN;

    return ina219_calc_current_mA(reg, hw->current_lsb);
}

int ina219_read_data(ina219_t* hw, ina219_data_t* data)
{
    int err;

    err = ina219_read_reg(hw, INA219_REG_BUS, &data->bus);
    if (err < 0)
        return err;

    err = ina219_read_reg(hw, INA219_REG_CURRENT, &data->current);
    if (err < 0)
        return err;

    // Read power last because reading it clears BUS_CNVR
    err = ina219_read_reg(hw, INA219_REG_POWER, &data->power);
    if (err < 0)
        return err;

    data->current_lsb = hw->current_lsb;
    data->power_lsb = hw->power_lsb;

    return PICO_OK;
}

bool ina219_data_overflowed(const ina219_data_t* data)
{
    return data->bus & INA219_BUS_OVF;
}

bool ina219_data_ready(const ina219_data_t* data)
{
    return data->bus & INA219_BUS_CNVR;
}

bool ina219_data_shunt_clipped(const ina219_data_t* data)
{
    const int16_t current = data->current;
    const int16_t nearly_full_scale = 0x7FF8;

    return (current > nearly_full_scale)
        || (current < -nearly_full_scale);
}

bool ina219_data_bus_clipped(const ina219_data_t* data)
{
    const uint16_t bus = data->bus >> 3;
    const int16_t nearly_full_scale_16V = 0xF98;

    return bus > nearly_full_scale_16V;
}

float ina219_data_bus_V(const ina219_data_t* data)
{
    return ina219_calc_bus_V(data->bus);
}

float ina219_data_power_mW(const ina219_data_t* data)
{
    return ina219_calc_power_mW(data->power, data->power_lsb);
}

float ina219_data_current_mA(const ina219_data_t* data)
{
    return ina219_calc_current_mA(data->current, data->current_lsb);
}

uint32_t ina219_adc_conversion_us(enum ina219_adc adc)
{
    switch (adc) {
    case INA219_ADC_BITS_9:         return 84;
    case INA219_ADC_BITS_10:        return 148;
    case INA219_ADC_BITS_11:        return 276;
    case INA219_ADC_BITS_12:        return 532;
    case INA219_ADC_SAMPLES_2:      return 1060;
    case INA219_ADC_SAMPLES_4:      return 2130;
    case INA219_ADC_SAMPLES_8:      return 4260;
    case INA219_ADC_SAMPLES_16:     return 8510;
    case INA219_ADC_SAMPLES_32:     return 17020;
    case INA219_ADC_SAMPLES_64:     return 34050;
    case INA219_ADC_SAMPLES_128:    return 68100;
    }
}

uint32_t ina219_cfg_conversion_us(const ina219_cfg_t* cfg)
{
    return ina219_adc_conversion_us(cfg->bus_adc) 
         + ina219_adc_conversion_us(cfg->shunt_adc);
}

uint32_t ina219_conversion_us(const ina219_t* hw)
{
    ina219_cfg_t cfg;
    ina219_calc_config(hw->cfg, &cfg);
    return ina219_cfg_conversion_us(&cfg);
}
