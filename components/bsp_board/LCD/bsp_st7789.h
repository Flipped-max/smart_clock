#ifndef __BSP_ST7789_H
#define __BSP_ST7789_H

#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#define BSP_LCD_PIXEL_CLK 40*1000*1000 //刷新频率40MHz
#define BSP_LCD_PIN_CS GPIO_NUM_10
#define BSP_LCD_PIN_DC GPIO_NUM_9
#define BSP_LCD_PIN_RST -1
#define BSP_LCD_PIN_BK -1

//刷新频率
#define BSP_LCD_V_RES 296
#define BSP_LCD_H_RES 240
#define BSP_LCD_DRAW_BUFF_HEIGHT 30 //LVGL分块刷新高度

esp_err_t bsp_lcd_init(esp_lcd_panel_io_handle_t *ret_io, esp_lcd_panel_handle_t *ret_panel);

#endif