#include "bsp_st7789.h"
#include "bsp_spi.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"


static const char *TAG = "ST7789";

esp_err_t bsp_lcd_init(esp_lcd_panel_io_handle_t *ret_io, esp_lcd_panel_handle_t *ret_panel)
{
    //初始化SPI总线
    ESP_ERROR_CHECK(bsp_spi_init(BSP_LCD_H_RES * BSP_LCD_DRAW_BUFF_HEIGHT * sizeof(uint16_t)));

    ESP_LOGW(TAG,"初始化IO句柄");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = BSP_LCD_PIN_CS,
        .dc_gpio_num = BSP_LCD_PIN_DC,
        .pclk_hz = BSP_LCD_PIXEL_CLK,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    //将LCD挂载到SPI总线
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_SPI_HOST,
                             &io_cfg,
                             &io_handle);
    ESP_LOGW(TAG,"安装ST7789面板驱动");

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BSP_LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle,&panel_cfg,&panel_handle));
 
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    //esp_lcd_panel_set_gap(panel_handle,0,24);
     //配置屏幕属性
    //st7789需要反转颜色才能正常显色
    esp_lcd_panel_invert_color(panel_handle,true);
    //打开显示
    esp_lcd_panel_disp_on_off(panel_handle,true);

    //将句柄到出给上层(app_ui)
    if(ret_io)    *ret_io = io_handle;
    if(ret_panel) *ret_panel = panel_handle;
    
    return ESP_OK;
}
