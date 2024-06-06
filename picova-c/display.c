#include "FreeRTOS.h"
#include "task.h"
#include "display.h"
#include "i2c_dma.h"

u8g2_t u8g2;

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

int display_init_i2c(i2c_inst_t *i2c, uint baudrate, uint sda_gpio, uint scl_gpio)
{
    i2c_dma_t* i2c_dma = NULL;
    int rc = i2c_dma_init(&i2c_dma, i2c, baudrate, sda_gpio, scl_gpio);
    u8g2_SetUserPtr(&u8g2, i2c_dma);
    return rc;
}

int display_init_ssd1306(void)
{
    u8g2_SetupDisplay(&u8g2, u8x8_d_ssd1306_128x64_noname, cad_cb, i2c_cb, gpio_delay_cb);
    uint8_t tile_buf_height;
    uint8_t* buf = u8g2_m_16_8_f(&tile_buf_height);
    u8g2_SetupBuffer(&u8g2, buf, tile_buf_height, u8g2_ll_hvline_vertical_top_lsb, U8G2_R2);
}
