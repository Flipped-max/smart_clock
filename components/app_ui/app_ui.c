#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_littlefs.h"

#include "app_common.h"
#include "bsp_st7789.h"
#include "app_ui.h"

static const char *TAG = "APP_UI";

//UI控件句柄
static lv_obj_t *s_label_status = NULL; //顶部状态栏
static lv_obj_t *s_label_time   = NULL; //中间大号数字时钟
static lv_obj_t *s_label_chat   = NULL; //底部AI语音问答文本框

//存放流式追加文本的静态缓冲
static char s_chat_txt_buf[512] = {0};

lv_font_t *font_cn_16 = NULL;

//1秒周期定时器，自动刷新表盘时间
static void clock_timer_cb(lv_timer_t *timer)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    lv_label_set_text_fmt(s_label_time, "%02d:%02d:%02d",
                          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

//绘制主界面(在lvgl_port_lock保护下调用)
static void ui_create_clock_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    //设置深色背景
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x121212), 0);

    //选择中文字体

    const lv_font_t *font_cn = (font_cn_16 != NULL) ? font_cn_16 : LV_FONT_DEFAULT;

    //1.顶部状态栏
    s_label_status = lv_label_create(scr);
    lv_obj_set_style_text_font(s_label_status, font_cn, 0);
    lv_obj_set_style_text_color(s_label_status, lv_color_hex(0x00E676), 0);
    lv_label_set_text(s_label_status, "待机中");
    lv_obj_align(s_label_status, LV_ALIGN_TOP_MID, 0, 12);

    //中间数字时钟
    s_label_time = lv_label_create(scr);
#if LV_FONT_MONTSERRAT_48
    lv_obj_set_style_text_font(s_label_time, &lv_font_montserrat_48, 0);
#else
    lv_obj_set_style_text_font(s_label_time, LV_FONT_DEFAULT, 0);
#endif
    lv_obj_set_style_text_color(s_label_time, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(s_label_time, "12:00:00");
    lv_obj_align(s_label_time, LV_ALIGN_TOP_MID, 0, 50);

    //3.底部AI对话卡片容器(220 * 150)
    lv_obj_t *chat_panel = lv_obj_create(scr);
    lv_obj_set_size(chat_panel, BSP_LCD_H_RES -16, 155);
    lv_obj_align(chat_panel, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(chat_panel, lv_color_hex(0x1E1E2E), 0);
    lv_obj_set_style_border_color(chat_panel, lv_color_hex(0x3A3A5A), 0);
    lv_obj_set_style_radius(chat_panel, 8, 0);
    lv_obj_set_style_pad_all(chat_panel, 8, 0);

    //对话文本Label(支持自动换行)
    s_label_chat = lv_label_create(chat_panel);
    lv_obj_set_width(s_label_chat, lv_pct(100));
    lv_label_set_long_mode(s_label_chat, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_label_chat, font_cn, 0);
    lv_obj_set_style_text_color(s_label_chat, lv_color_hex(0xE0E0E0), 0);
    lv_label_set_text(s_label_chat, "对麦克风说：\nHi 乐鑫");

    //创建1秒定时器刷新时间
    lv_timer_create(clock_timer_cb, 1000, NULL);
}

//UI消息队列消费任务 将语音/AI同步到屏幕
static void ui_msg_consumer_task(void *arg)
{
    ui_msg_t msg;
    while(1)
    {
        if(xQueueReceive(g_ui_msg_queue, &msg, portMAX_DELAY) == pdTRUE)
        {
            //所有LVGL API调用必须加锁
            if(lvgl_port_lock(-1))
            {
                switch (msg.type)
                {
                    case UI_EVT_STATE_IDLE:
                        lv_obj_set_style_text_color(s_label_status, lv_color_hex(0x00E676), 0);
                        lv_label_set_text(s_label_status, "待机中");
                        break;
                    
                    case UI_EVT_STATE_LISTENING:
                        lv_obj_set_style_text_color(s_label_status, lv_color_hex(0xFF5252), 0);
                        lv_label_set_text(s_label_status, ">>>正在倾听....");
                        lv_label_set_text(s_label_chat, "请说话");
                        break;
                    
                    case UI_EVT_STATE_THINKING:
                        lv_obj_set_style_text_color(s_label_status, lv_color_hex(0xFFD740),0);
                        lv_label_set_text(s_label_status, "正在思考");
                        break;

                    case UI_EVT_TEXT_CLEAR:
                        s_chat_txt_buf[0] = '\0';
                        lv_label_set_text_static(s_label_chat, s_chat_txt_buf);
                        break;
                        
                    case UI_EVT_TEXT_ASR_RESULT:
                        snprintf(s_chat_txt_buf, sizeof(s_chat_txt_buf), "问：%s\n答：", msg.text);
                        lv_label_set_text_static(s_label_chat, s_chat_txt_buf);
                        break;
                    
                    case UI_EVT_TEXT_LLM_APPEND:
                        strncat(s_chat_txt_buf, msg.text, sizeof(s_chat_txt_buf) - strlen(s_chat_txt_buf) - 1);
                        lv_label_set_text_static(s_label_chat, s_chat_txt_buf);
                        break;
                    
                    default:
                        break;
                }
                lvgl_port_unlock();
            }
        }
    }
}

//声明全局字体指针


esp_err_t app_ui_init_and_start(void)
{
    //挂载LittleFS到VFS的/S节点
    esp_vfs_littlefs_conf_t conf = 
    {
        .base_path = "/S",
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if(ret != ESP_OK)
    {
        ESP_LOGE("FS", "Failed to mount LittleFS (%s)",esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI("FS","LittleFS mounted successfully");

    //初始化st7789硬件并获取句柄
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_handle_t panel_handle = NULL;

    bsp_lcd_init(&io_handle,&panel_handle);
    //初始化esp_lvgl_port任务与定时器
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    //将st7789注册到LVGL
    const lvgl_port_display_cfg_t disp_cfg = 
    {
        .io_handle      = io_handle,
        .panel_handle   = panel_handle,
        .buffer_size    = BSP_LCD_H_RES * BSP_LCD_DRAW_BUFF_HEIGHT,
        .double_buffer  = true,
        .hres           = BSP_LCD_H_RES,
        .vres           = BSP_LCD_V_RES,
        .monochrome     = false,
        .rotation = {
            .swap_xy  = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,   //使用内部DMA内存
        }
    };
    lvgl_port_add_disp(&disp_cfg);

    FILE *f = fopen("/S/font_cn_16.bin","rb");
    if(f==NULL)
    {
        ESP_LOGE("FS_CHECK","VFS底层找不到");
    }
    else
    {
        fseek(f,0,SEEK_END);
        long size = ftell(f);
        fclose(f);
        ESP_LOGI("FS_CHECK","VFS底层读取字库成功，文件大小：%ld 字节",size);
    }


    if(lvgl_port_lock(-1))
    {
        font_cn_16 = lv_font_load("S:/S/font_cn_16.bin");

        if(font_cn_16 == NULL)
        {
            ESP_LOGE("UI","加载字库font_cn_16_bin失败");
        }
        else
        {
            ESP_LOGI("UI","二进制字库加载成功");
        }

        lvgl_port_unlock();
    }
    //加载并绘制初始界面
    if(lvgl_port_lock(-1))
    {
        ui_create_clock_screen();
        lvgl_port_unlock();
    }

    //启动UI消息队列消费者任务
    xTaskCreatePinnedToCore(ui_msg_consumer_task, "ui_consumer", 4096, NULL, 5, NULL, 0);
    ESP_LOGI(TAG, "LVGL UI initialized and consumer task started.");
    return ESP_OK;
}
