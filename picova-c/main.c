#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdio_usb.h"
#include "pico/time.h"
#include "i2c_dma.h"
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
static QueueHandle_t meas_queue = NULL;
static QueueHandle_t display_queue = NULL;
static u8g2_t u8g2;

struct measurement
{
    uint32_t timestamp;
    ina219_data_t data;
};

struct avg_measurement
{
    float V, mA, mW;
};

static bool on_read_timer(repeating_timer_t* timer)
{
    TaskHandle_t read_task = timer->user_data;
    xTaskNotifyGive(read_task);
    return true;
}

static void on_disp_timer(TimerHandle_t timer)
{
    TaskHandle_t write_task = pvTimerGetTimerID(timer);
    xTaskNotifyGive(write_task);
}

// Like u8x8_cad_ssd13xx_fast_i2c but don't chunk data into < 32 bytes.
static uint8_t cad_cb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr)
{
    static bool in_transfer = false;

    switch (msg) {
    case U8X8_MSG_CAD_INIT:
        if (u8x8->i2c_address == 0xFF)
            u8x8->i2c_address = 0x78;
        break;

    case U8X8_MSG_CAD_START_TRANSFER:
        in_transfer = false;
        break;

    case U8X8_MSG_CAD_END_TRANSFER:
        if (in_transfer)
            u8x8_byte_EndTransfer(u8x8);
        in_transfer = false;
        break;

    case U8X8_MSG_CAD_SEND_CMD:
        if (!in_transfer) {
            u8x8_byte_StartTransfer(u8x8);
            u8x8_byte_SendByte(u8x8, 0x00);
            in_transfer = true;
        }
        u8x8_byte_SendByte(u8x8, arg_int);
        break;

    case U8X8_MSG_CAD_SEND_ARG:
        u8x8_byte_SendByte(u8x8, arg_int);
        break;

    case U8X8_MSG_CAD_SEND_DATA:
        if (in_transfer)
            u8x8_byte_EndTransfer(u8x8);

        u8x8_byte_StartTransfer(u8x8);
        u8x8_byte_SendByte(u8x8, 0x40);
        u8x8_byte_SendBytes(u8x8, arg_int, arg_ptr);
        u8x8_byte_EndTransfer(u8x8);
        in_transfer = false;
        break;

    default:
        return 0;
    }

    return 1;
}

static uint8_t i2c_cb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr)
{
    static i2c_dma_xfer_t xfer = NULL;
    i2c_dma_t* const i2c_dma = u8x8_GetUserPtr(u8x8);

    switch (msg) {
    case U8X8_MSG_BYTE_START_TRANSFER:
        xfer = i2c_dma_xfer_init(i2c_dma, u8g2_GetI2CAddress(&u8g2) >> 1);
        if (!xfer)
            return 0;
        break;

    case U8X8_MSG_BYTE_SEND:
        if (i2c_dma_xfer_write(xfer, arg_ptr, arg_int) != PICO_OK) {
            i2c_dma_xfer_abort(xfer);
            xfer = NULL;
            return 0;
        }
        break;

    case U8X8_MSG_BYTE_END_TRANSFER:
        if (i2c_dma_xfer_execute(xfer) != PICO_OK)
            return 0;
        xfer = NULL;
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
        vTaskDelay(pdMS_TO_TICKS(arg_int));
        break;

    default:
        return 0;
    }

    return 1;
}

// Read measurements from the INA219 as fast as possible and push them into a
// queue. This is the only task running on core 1.
static void read_task(void* arg)
{
    const ina219_cfg_t cfg = {
        .bus_range   = INA219_BUS_RANGE_16V,
        .shunt_range = INA219_SHUNT_RANGE_40mV,
        .bus_adc     = INA219_ADC_BITS_9,
        .shunt_adc   = INA219_ADC_BITS_11,
    };

    ina219_init(&ina219, I2C_INA219, INA219_ADDR_DEFAULT, SHUNT_OHMS);
    ina219_reset(&ina219);
    ina219_configure(&ina219, &cfg);
    ina219_calibrate(&ina219);

    // Use a repeating timer on this core to initiate reads at the INA219's
    // conversion rate.
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    alarm_pool_t* alarm_pool = alarm_pool_create_with_unused_hardware_alarm(1);
    int64_t read_period = ina219_conversion_us(&ina219);
    struct repeating_timer read_timer;
    alarm_pool_add_repeating_timer_us(alarm_pool, -read_period, on_read_timer, task, &read_timer);

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

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

        xQueueSendToBack(meas_queue, &m, portMAX_DELAY);
    }
}

// Write the measurements out over stdio (USB CDC). Also accumulate averages to
// display periodically on the OLED.
static void write_task(void* arg)
{
    // Show the splash screen for a while.
    vTaskDelay(pdMS_TO_TICKS(1500));
    gpio_put(PIN_LED, 1);

    struct measurement m;
    struct avg_measurement avg = {0};
    size_t avg_num = 0;

    // Periodically display the averaged measurement on the display.
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    TimerHandle_t disp_timer = xTimerCreate("disp", pdMS_TO_TICKS(250), pdTRUE, task, on_disp_timer);
    xTimerStart(disp_timer, 0);

    while (true) {
        xQueueReceive(meas_queue, &m, portMAX_DELAY);
        const float V = ina219_data_bus_V(&m.data);
        const float mA = ina219_data_current_mA(&m.data);
        const float mW = ina219_data_power_mW(&m.data);

        printf("%lu,%f,%f,%f\n", m.timestamp, V, mA, mW);

        avg.V += V;
        avg.mA += mA;
        avg.mW += mW;
        avg_num += 1;

        if (ulTaskNotifyTake(pdTRUE, 0)) {
            avg.V /= avg_num;
            avg.mA /= avg_num;
            avg.mW /= avg_num;

            xQueueSendToBack(display_queue, &avg, 0);

            avg.V = 0;
            avg.mA = 0;
            avg.mW = 0;
            avg_num = 0;
        }
    }
}

// Display averaged readings on the OLED.
static void display_task(void* arg)
{
    struct avg_measurement m;
    char str[11];

    u8g2_SetupDisplay(&u8g2, u8x8_d_ssd1306_128x64_noname, cad_cb, i2c_cb, gpio_delay_cb);
    uint8_t tile_buf_height;
    uint8_t* buf = u8g2_m_16_8_f(&tile_buf_height);
    u8g2_SetupBuffer(&u8g2, buf, tile_buf_height, u8g2_ll_hvline_vertical_top_lsb, U8G2_R2);

    u8g2_InitDisplay(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont22_tr);
    u8g2_ClearBuffer(&u8g2);
    u8g2_DrawStr(&u8g2, 28, 40, "PicoVA");
    u8g2_SendBuffer(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);

    while (true) {
        xQueueReceive(display_queue, &m, portMAX_DELAY);

        u8g2_ClearBuffer(&u8g2);
        snprintf(str, sizeof(str), "%7.3f V", m.V);
        u8g2_DrawStr(&u8g2, 0, 18, str);
        snprintf(str, sizeof(str), "%7.3f mA", m.mA);
        u8g2_DrawStr(&u8g2, 0, 40, str);
        snprintf(str, sizeof(str), "%7.3f mW", m.mW);
        u8g2_DrawStr(&u8g2, 0, 62, str);
        u8g2_SendBuffer(&u8g2);
    }
}

static void __attribute__((noreturn)) die(const char* msg)
{
    puts(msg);
    while (true) {
        gpio_put(PIN_LED, 1);
        sleep_ms(250);
        gpio_put(PIN_LED, 0);
        sleep_ms(250);
    }
}

int main()
{
    stdio_usb_init();

    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);

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

    i2c_dma_t* i2c_dma = NULL;
    i2c_dma_init(&i2c_dma, I2C_SSD1306, 1000000, PIN_SDA_SSD1306, PIN_SCL_SSD1306);
    u8g2_SetUserPtr(&u8g2, i2c_dma);

    // Set up tasks and IPC
    meas_queue = xQueueCreate(255, sizeof(struct measurement));
    if (!meas_queue) {
        die("Failed to create measurement queue");
    }

    display_queue = xQueueCreate(2, sizeof(struct avg_measurement));
    if (!display_queue) {
        die("Failed to create display queue");
    }

    // TODO: run read_task on core 1. Currently the system locks up when
    // read_task is run on core 1.
    BaseType_t ret = xTaskCreateAffinitySet(read_task, "read", 1024, NULL, configMAX_PRIORITIES - 1, 1 << 0, NULL);
    if (ret != pdPASS) {
        die("Failed to create read task on core 1");
    }

    ret = xTaskCreateAffinitySet(write_task, "write", 1024, NULL, tskIDLE_PRIORITY + 1, 1 << 0, NULL);
    if (ret != pdPASS) {
        die("Failed to create write task on core 0");
    }

    ret = xTaskCreateAffinitySet(display_task, "display", 1024, NULL, tskIDLE_PRIORITY + 1, 1 << 0, NULL);
    if (ret != pdPASS) {
        puts("Failed to create display task on core 0");
    }

    vTaskStartScheduler();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    die("Stack overflow");
}
