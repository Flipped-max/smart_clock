#ifndef __BSP_SPI_H
#define __BSP_SPI_H

#include "driver/spi_master.h"
#include "esp_err.h"
#include "driver/gpio.h"

#define BSP_SPI_HOST SPI2_HOST

#define BSP_SPI_PIN_CLK GPIO_NUM_12
#define BSP_SPI_PIN_MOSI GPIO_NUM_11
#define BSP_SPI_PIN_MISO -1

esp_err_t bsp_spi_init(size_t max_transfer_sz);

#endif


