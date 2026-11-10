#include "bsp_spi.h"
#include "esp_log.h"

static const char *TAG = "BSP_SPI";
static bool is_spi_initialized = false;

esp_err_t bsp_spi_init(size_t max_transfer_sz)
{
    if(is_spi_initialized==true)
    {
        ESP_LOGW(TAG, "BSP_SPI has initialied");
        return ESP_OK;
    }
    spi_bus_config_t spi_cfg = {
        .sclk_io_num = BSP_SPI_PIN_CLK,
        .mosi_io_num = BSP_SPI_PIN_MOSI,
        .miso_io_num = BSP_SPI_PIN_MISO,
        .quadhd_io_num = -1,
        .quadwp_io_num = -1,
        .max_transfer_sz = max_transfer_sz,
    };

    esp_err_t ret = (spi_bus_initialize(BSP_SPI_HOST,&spi_cfg,SPI_DMA_CH_AUTO));
    if(ret==ESP_OK)
    {
        is_spi_initialized = true;
        ESP_LOGW(TAG, "BSP_SPI has initialized and DMA successfully");
    }

    return ret;
}

