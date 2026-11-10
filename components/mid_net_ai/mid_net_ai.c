#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "cJSON.h"

#include "app_common.h"
#include "mid_net_ai.h"

extern int16_t *g_psram_res_buf;
extern size_t   g_rec_bytes;

static const char *TAG = "MID_NET_AI";
static bool s_sntp_initialized = false;

//辅助函数，向UI消息队列发送事件更新屏幕
static void send_ui_event(ui_event_type_t type, const char *text)
{
    if(g_ui_msg_queue == NULL) return;
    ui_msg_t msg = 
    {
        .type = type,
    };
    if(text != NULL)
    {
        strncpy(msg.text, text, sizeof(msg.text) - 1);
        msg.text[sizeof(msg.text) -1] = '\0';
    }
    xQueueSend(g_ui_msg_queue, &msg, pdMS_TO_TICKS(50));
}

/*================WMO国际天气代码中文描述==============*/
static const char* wmo_code_to_sr(int code)
{
    if(code == 0) return "晴朗";
    if(code >= 1 && code <= 3) return "多云";
    if(code == 45 || code == 48) return "有雾";
    if(code >= 51 && code <= 67) return "降雨";
    if(code >= 71 && code <= 77) return "降雪";
    if(code >= 80 && code <= 82) return "阵雨";
    if(code >= 95) return "雷暴";
    return "晴间多云";
}

//SNTP时间同步成功后的回调函数
static void time_sync_notification_cb(struct timeval *tv)
{
    time_t now = 0;
    struct tm timeinfo = {0};
    time(&now);
    localtime_r(&now, &timeinfo);

    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "SNTR时间同步成功，当前北京时间%s", strftime_buf);
}

//启动或重启SNTR校时服务
static void start_sntp_sync(void)
{
    //设置中国标准时间(UTC+8)
    setenv("TZ", "CST-8", 1);
    tzset();

    if(!s_sntp_initialized)
    {
        ESP_LOGI(TAG, "Initializing SNTP service");
        
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
        config.sync_cb = time_sync_notification_cb;
        esp_netif_sntp_init(&config);
        s_sntp_initialized = true;
    }
    else
    {
        esp_netif_sntp_start();
    }
}

//WiFi与IP事件回调处理函数
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "WiFi STA Started, connecting to AP %s ..", WIFI_STA_SSID);
        esp_wifi_connect();
    }
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if(g_sys_event_group != NULL)
        {
            xEventGroupClearBits(g_sys_event_group, WIFI_CONNECTED_BIT);
        }
        ESP_LOGW(TAG, "WiFi断开连接，正在尝试自动重启");
        esp_wifi_connect();
    }
    else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "WiFi连接成功 获取到IP："IPSTR, IP2STR(&event->ip_info.ip));

        if(g_sys_event_group != NULL)
        {
            xEventGroupSetBits(g_sys_event_group, WIFI_CONNECTED_BIT);
        }

        //获取IP后立即重启SNTR时间同步
        start_sntp_sync();
    }
}

/*================Open-Meteo 免费天气请求=============*/
typedef struct {
    char *buf;
    size_t len;
    size_t cap;
}http_resp_buf_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_resp_buf_t *resp = (http_resp_buf_t *)evt->user_data;
    if(evt->event_id == HTTP_EVENT_ON_DATA && resp && resp->buf)
    {
        if(resp->len + evt->data_len < resp->cap - 1)
        {
            memcpy(resp->buf + resp->len, evt->data, evt->data_len);
            resp->len += evt->data_len;
            resp->buf[resp->len] = '\0';
        }
    }
    return ESP_OK;
}

/**
 * @brief 请求Open-Meteo API并刷新解析出实时天气以流式打印机效果输出到屏幕
 */
static esp_err_t fetch_open_meteo_and_display(void)
{
    http_resp_buf_t resp = 
    {
        .buf = (char*)heap_caps_calloc(1, 2048, MALLOC_CAP_SPIRAM),
        .len = 0,
        .cap = 2048
    };
    if(!resp.buf) return ESP_ERR_NO_MEM;

    esp_http_client_config_t config = {
        .url = OPEN_METEO_URL,
        .method = HTTP_METHOD_GET,
        .event_handler = http_event_handler,
        .user_data = &resp,
        .crt_bundle_attach = esp_crt_bundle_attach,//启用内置HTTPS根证书
        .timeout_ms = 10000,
    };

    ESP_LOGI(TAG, "正在请求Open-Meteo 免费天气接口：%s", OPEN_METEO_URL);
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if(err == ESP_OK)
    {
        ESP_LOGI(TAG, "Open-Meteo 原始响应:%s", resp.buf);

        cJSON *root = cJSON_Parse(resp.buf);
        cJSON *current = root ? cJSON_GetObjectItem(root, "current") : NULL;
        if(current)
        {
            cJSON *temp_item = cJSON_GetObjectItem(current, "temperature_2m");
            cJSON *code_item = cJSON_GetObjectItem(current, "weather_code");
            cJSON *wind_item = cJSON_GetObjectItem(current, "wind_speed_10m");

            double temp = cJSON_IsNumber(temp_item) ? temp_item->valuedouble : 0.0;
            int    code = cJSON_IsNumber(code_item) ? code_item->valueint    : 0;
            double wind = cJSON_IsNumber(wind_item) ? wind_item->valuedouble : 0.0;

            //将解析结果拆分为多个小片段，通过UI队列演示流式打字追加效果
            char chunk1[64], chunk2[64], chunk3[64];
            snprintf(chunk1, sizeof(chunk1), "[%s实时天气]", OPEN_METEO_CITY);
            snprintf(chunk2, sizeof(chunk2), "%s, 气温 %.1f 摄氏度", wmo_code_to_sr(code), temp);
            
            send_ui_event(UI_EVT_TEXT_LLM_APPEND, chunk1);
            vTaskDelay(pdMS_TO_TICKS(180));
            send_ui_event(UI_EVT_TEXT_LLM_APPEND, chunk2);
            vTaskDelay(pdMS_TO_TICKS(180));
            send_ui_event(UI_EVT_TEXT_LLM_APPEND, chunk3);
        }
        else
        {
            send_ui_event(UI_EVT_TEXT_LLM_APPEND, "JSON解析失败");
        }
        if(root) cJSON_Delete(root);
    }
    else
    {
        ESP_LOGE(TAG, "Open-Meteo HTTP 请求失败：%s", esp_err_to_name(err));
        send_ui_event(UI_EVT_TEXT_LLM_APPEND, "网络请求超时，请检查网络连接");
    }
    esp_http_client_cleanup(client);
    free(resp.buf);
    return err;
}



/**
 * @brief 云端AI交互主任务
 * 负责监听PSRAM录音完成事件，并调用云端ASR + LLM接口
 */
static void net_ai_task(void *arg)
{
    ESP_LOGI(TAG, "Open-Meteo Cloud Verify Task started on Coure 0");

    //1.开机首次等待WiFi连接成功，自动拉取一次天气显示在待机界面
    xEventGroupWaitBits(g_sys_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "首次联网成功，自动执行一次Open-Meteo天气校验");
    fetch_open_meteo_and_display();

    while(1)
    {
        //1.阻塞等待mid_audio_sr置位AUDIO_REC_DONE_BIT
        EventBits_t bits = xEventGroupWaitBits(g_sys_event_group,
                                               AUDIO_REC_DONE_BIT,
                                               pdTRUE,
                                               pdFALSE,
                                               portMAX_DELAY);
        if(bits & AUDIO_REC_DONE_BIT)
        {
            float rec_sec = (float)g_rec_bytes / (SAMPLE_RATE * sizeof(int16_t));
            ESP_LOGI(TAG, "收到录音完成信号，准备处理PSRAM中的%zu字节音频...",g_rec_bytes);

            //检查网络是否已连接
            if(!(xEventGroupGetBits(g_sys_event_group) & WIFI_CONNECTED_BIT))
            {
                send_ui_event(UI_EVT_TEXT_CLEAR, NULL);
                send_ui_event(UI_EVT_TEXT_LLM_APPEND, "WiFi 未连接");
                vTaskDelay(pdMS_TO_TICKS(2000));
                xEventGroupSetBits(g_sys_event_group, AI_PROCESS_DONE_BIT);
                continue;
            }

            //在屏幕显示录音时长与模拟提问
            char prompt_info[64];
            snprintf(prompt_info, sizeof(prompt_info), "语音指令(%.1f秒):查询天气", rec_sec);
            send_ui_event(UI_EVT_TEXT_ASR_RESULT, prompt_info);

            //请求Open-Meteo实时天气并流式显示到屏幕
            fetch_open_meteo_and_display();

            //停留1.5秒后通知mide_aduio_sr恢复待机唤醒监听
            vTaskDelay(pdMS_TO_TICKS(1500));
            xEventGroupSetBits(g_sys_event_group, AI_PROCESS_DONE_BIT);
        }
    }   
}

esp_err_t mid_net_ai_init_and_start(void)
{
    //1.初始化NVS
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret ==ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //2.初始化TCP/IP与系统事件循环
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    //初始化WiFi底层驱动
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //注册 WiFi 与 IP事件回调
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    //配置并启动WiFi STA模式
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_STA_SSID,
            .password = WIFI_STA_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_wifi_set_ps(WIFI_PS_NONE);

    //创建云端任务
    xTaskCreatePinnedToCore(net_ai_task, "ent_ai_task", 10240, NULL, 5, NULL, 0);

    ESP_LOGI(TAG, "Network, SNTP & Open-Meteo moudule initialized");
    return ESP_OK;
}

