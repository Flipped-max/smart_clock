#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "model_path.h"

#include "mid_audio_sr.h"
#include "inmp441.h"
#include "app_common.h"

static const char *TAG = "MID_AUDIO_SR";

//定义全局共享PSRAM录音缓冲区与有效字节数
int16_t *g_psram_rec_buf = NULL;
size_t   g_rec_bytes     = 0;

//AFE句柄与数据指针
static const esp_afe_sr_iface_t *s_afe_handle = NULL;
static esp_afe_sr_data_t        *s_afe_data   = NULL;
static srmodel_list_t           *s_models     = NULL;

//内部状态机 标记当前是否处于唤醒后的录音阶段
typedef enum
{
    SR_STATE_WAIT_WAKEUP = 0,//待机监听唤醒词
    SR_STATE_RECORDING,      //已唤醒，正在向PSRAM写入用户语音
}sr_work_state_t;

static volatile sr_work_state_t s_sr_state = SR_STATE_WAIT_WAKEUP;

//向UI消息队列发送事件更新屏幕
static void send_ui_event(ui_event_type_t type, const char *text)
{
    if(g_ui_msg_queue == NULL) return;
    ui_msg_t msg = { .type = type};
    if(text != NULL)
    {
        strncpy(msg.text, text, sizeof(msg.text) - 1);
        msg.text[sizeof(msg.text) -1] = '\0';
    }
    xQueueSend(g_ui_msg_queue, &msg, pdMS_TO_TICKS(50));
}

/*====任务1：音频喂数据任务(Feed Task)====
*持续从INMP441读取16bit PCM数据并送入ESP-SR AFE引擎
*/
static void audio_feed_task(void *arg)
{
    int audio_chunksize = s_afe_handle->get_feed_chunksize(s_afe_data);
    int feed_channel    = s_afe_handle->get_channel_num(s_afe_data);
    size_t buf_size     = audio_chunksize * feed_channel * sizeof(int16_t);

    //分配内部DMA友好的PAM用于高频读取I2S
    int16_t *i2s_buff = (int16_t *)heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if(!i2s_buff)
    {
        ESP_LOGE(TAG, "Failed to allocate I2S feed buffer");
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG,"Audio Feed Task started *chunk=%d samples, ch=%d",audio_chunksize, feed_channel);
    
    while(1)
    {
        //调用底层INMP441接口读取16bit数据
        esp_err_t ret = bsp_i2s_mic_read_16bit(i2s_buff, audio_chunksize * feed_channel, 1000);
        if(ret == ESP_OK)
        {
            s_afe_handle->feed(s_afe_data, i2s_buff);
        }else{
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

/**
 * 任务二：模型检测与录音状态机任务(Detect Task)
 * 从AFE获取唤醒后的音频，检测唤醒词，并在唤醒或将语音录制到PSRAM
 */
static void audio_detect_task(void *arg)
{
    int afe_chunksie = s_afe_handle->get_fetch_chunksize(s_afe_data);
    size_t chunk_bytes = afe_chunksie *sizeof(int16_t);

    //VAD智能断句计数器
    bool has_spoken = false;
    int silence_chunks = 0;
    //AFE每次fetch约32ms25个chunk约等于0.8秒静音
    const int MAX_SILENCE_CHUNKS = 25;

    ESP_LOGI(TAG, "Audio Detect Task started(fetch_chunk=%d samples)",afe_chunksie);

    while(1)
    {
        afe_fetch_result_t *res = s_afe_handle->fetch(s_afe_data);
        if(!res || res->ret_value == ESP_FAIL)
        {
            continue;
        }

        /*--------状态1：等待语音唤醒-------*/
        if(s_sr_state == SR_STATE_WAIT_WAKEUP)
        {
            if(res->wakeup_state == WAKENET_DETECTED)
            {
                ESP_LOGW(TAG, ">>>唤醒成功(Wake word detected) <<<");

                //1.临时关闭换新词引擎，专心录音节省算力
                s_afe_handle->disable_wakenet(s_afe_data);

                //2.初始化录音参数
                g_rec_bytes = 0;
                has_spoken = false;
                silence_chunks = 0;
                s_sr_state = SR_STATE_RECORDING;

                //3.发送消息通知LVGL屏幕切换为红色
                send_ui_event(UI_EVT_STATE_LISTENING, NULL);
            }
        }
        /*---------状态2：唤醒后录制用户指令--------*/
        else if(s_sr_state == SR_STATE_RECORDING)
        {
            //将AFE降噪后的纯净人声追加写入PSRAM缓冲区
            if(g_rec_bytes + chunk_bytes <= PSRAM_REC_BUF_SIZE)
            {
                memcpy((uint8_t *)g_psram_rec_buf + g_rec_bytes, res->data, chunk_bytes);
                g_rec_bytes += chunk_bytes;
            }

            //检测用户是否开始说话(利用AFE内置的VAD状态)
            if(res->vad_state == AFE_VAD_SPEECH)
            {
                has_spoken = true;
                silence_chunks = 0;//只要在说话，静音计数归零
            }
            else if(has_spoken && res->vad_state == AFE_VAD_SILENCE)
            {
                silence_chunks++; //说完话后开始静音计时
            }

            /**
             * 结束录音判定条件：
             * 条件A：已经说过话，且连续静音超过0.8秒
             * 条件B：达到最大录音时长5s
             */
            bool vad_stop = (has_spoken && (silence_chunks >= MAX_SILENCE_CHUNKS) && (g_rec_bytes > 32000));
            bool buf_full = (g_rec_bytes + chunk_bytes > PSRAM_REC_BUF_SIZE);

            if(vad_stop || buf_full)
            {
                ESP_LOGI(TAG, "录音完成！总字节数：%zu Bytes(约%.2f 秒)，触发原因：%s",
                         g_rec_bytes,
                         (float)g_rec_bytes / (SAMPLE_RATE * sizeof(int16_t)),
                         vad_stop ? "VAD智能短句" : "5秒超时满载");

                //.1通知UI切换为黄色"正在思考"
                send_ui_event(UI_EVT_STATE_THINKING, NULL);

                //2.通知全局事件组，触发阶段三的云端任务读取 g_psram_rec_buf
                if(g_sys_event_group != NULL)
                {
                    xEventGroupClearBits(g_sys_event_group, AI_PROCESS_DONE_BIT);
                    xEventGroupSetBits(g_sys_event_group,AUDIO_REC_DONE_BIT);

                    //3.阻塞等待云端ASR+LLM处理完毕(最长等待30秒超时保护)
                    ESP_LOGI(TAG, "等待云端AI任务处理完毕");
                    xEventGroupWaitBits(g_sys_event_group,
                                        AI_PROCESS_DONE_BIT,
                                        pdTRUE,
                                        pdFALSE,
                                        pdMS_TO_TICKS(30000));
                }
                else
                {
                    //若还未编写云端任务，延迟2秒模拟思考后直接恢复待机
                    vTaskDelay(pdMS_TO_TICKS(2000));
                }

                //4.云端处理结束，恢复待机状态与唤醒监听
                send_ui_event(UI_EVT_STATE_IDLE, NULL);
                s_afe_handle->enable_wakenet(s_afe_data);
                s_sr_state = SR_STATE_WAIT_WAKEUP;
                ESP_LOGI(TAG, "已重新开启唤醒词监听");
            }
        }
    }
}

esp_err_t mid_audio_sr_init_and_start(void)
{
    ESP_LOGI(TAG, "Initializing Audio SR Middleware...");

    //1.在外部8MB PSRAM中分配160k录音缓冲区
    if(g_psram_rec_buf == NULL)
    {
        g_psram_rec_buf = (int16_t *)heap_caps_malloc(PSRAM_REC_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if(g_psram_rec_buf == NULL)
        {
            ESP_LOGE(TAG, "Failed to allocate %d bytes in PSRAM", PSRAM_REC_BUF_SIZE);
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "Allocated %d KB recording buffer in PSRAM successfully", PSRAM_REC_BUF_SIZE);
    }
    //2.初始化底层INPM441 I2S麦克风硬件
    ESP_ERROR_CHECK(bsp_i2s_mic_init());

    //3.从model分区加载ESP-SR语音模型(WakeNet9)
    s_models = esp_srmodel_init("model");
    if(s_models == NULL)
    {
        ESP_LOGE(TAG, "Failed to init srmodel from 'model' partiton! Check partition.csv");
        return ESP_FAIL;
    }

    //4.配置AFE(Audio Front-End)参数
    afe_config_t afe_config = AFE_CONFIG_DEFAULT();
    afe_config.wakenet_model_name = esp_srmodel_filter(s_models, ESP_WN_PREFIX, NULL);
    afe_config.aec_init = false; //单麦克风无扬声器回声消除需求，关闭AEC节省算力
    afe_config.se_init = true; //开启语音增强与降噪
    afe_config.vad_init = true; //开启VAD人声检测用于智能短句
    afe_config.vad_mode = VAD_MODE_3; //灵敏度适中
    afe_config.wakenet_init = true;
    afe_config.wakenet_mode = DET_MODE_2CH_90;//高灵敏度唤醒
    afe_config.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;//优先使用PSRAM存放

    //配置麦克风通道
    afe_config.pcm_config.total_ch_num = 1;
    afe_config.pcm_config.mic_num = 1;
    afe_config.pcm_config.ref_num = 0;
    afe_config.pcm_config.sample_rate = SAMPLE_RATE;

    ESP_LOGI(TAG, "Loaded WakeNet model :%s",
             afe_config.wakenet_model_name ? afe_config.wakenet_model_name : "NULL");

    //5.创建AFE实例
    s_afe_handle = &ESP_AFE_SR_HANDLE;
    s_afe_data = s_afe_handle->create_from_config(&afe_config);
    if(s_afe_data == NULL)
    {
        ESP_LOGE(TAG, "Failed to create AFE instance");
        return ESP_FAIL;
    }

    //6.创建双任务并固定到Core1
    xTaskCreatePinnedToCore(audio_feed_task, "sr_feed", 8192, NULL, 6, NULL, 1);
    xTaskCreatePinnedToCore(audio_detect_task, "audio_detect", 8192, NULL, 5, NULL, 1);

    ESP_LOGI(TAG, "ESP_SR initialized and tasks started on Core1");
    return ESP_OK;
}
