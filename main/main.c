#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usart.h"
#include "inmp441.h"
#include "esp_err.h"
#include "esp_log.h"
#include "bsp_st7789.h"
#include "app_common.h"
#include "app_ui.h"
#include "mid_audio_sr.h"
#include "mid_net_ai.h"

static const char *TAG = "APP_Main";

//定义全局事件组与UI消息队列句柄
EventGroupHandle_t g_sys_event_group = NULL;
QueueHandle_t      g_ui_msg_queue    = NULL;

// static void send_ui_envent(ui_event_type_t type)
// {
//     if(g_ui_msg_queue == NULL)
//     {
//         ESP_LOGE(TAG,"msg_quene创建失败");
//         return;
//     }
//     ui_msg_t msg = 
//     {
//         .type = type,
//     };
//     xQueueSend(g_ui_msg_queue, &msg, pdMS_TO_TICKS(50));
// }

void app_main(void)
{
    
    g_sys_event_group = xEventGroupCreate();
    g_ui_msg_queue    = xQueueCreate(16, sizeof(ui_msg_t));

    //初始化st7789屏幕与LVGL时钟界面
    ESP_ERROR_CHECK(app_ui_init_and_start());

    //初始化INMP441并启动ESP-SR离线唤醒与VAD录音
    esp_err_t ret = mid_audio_sr_init_and_start();
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG,"Audio SR 启动失败");
    }
    ESP_LOGE(TAG,"Audio SR 启动正常");
    
    //初始化Wifi校时服务
    ESP_ERROR_CHECK(mid_net_ai_init_and_start());
    
    
    // while(1)
    // {
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }

}
