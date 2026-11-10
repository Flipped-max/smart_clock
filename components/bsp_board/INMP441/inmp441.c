#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "inmp441.h"
#include "app_common.h"

static i2s_chan_handle_t s_rx_chan=NULL;
static int32_t *s_dma_raw_32_buf = NULL;
static size_t s_raw_buf_samples = 0;

static const char *TAG = "INMP441";

//1.I2S标准模式初始化
esp_err_t bsp_i2s_mic_init(void)
{
    esp_err_t ret=ESP_OK;
    //Step1:分配I2S RX通道(使用I2S_NUM_0,由系统自动管理DMA缓冲)
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 6;
    chan_cfg.dma_frame_num = 240;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg,NULL,&s_rx_chan));

    //Step2:配置标准Philips I2S格式
    i2s_std_config_t std_cfg={
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, //IMP441内部时钟
            .bclk = BSP_INMP441_PIN_SCK,
            .ws = BSP_INMP441_PIN_WS,
            .dout = I2S_GPIO_UNUSED,
            .din = BSP_INMP441_PIN_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    //强制只采集左声道
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx_chan,&std_cfg));

    //Step3:使能I2S接收通道
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx_chan));
    ESP_LOGW(TAG,"INMP441 I2S Rx initialized successfully(16kHz, 32-bit Mono)");
    return ret;
}

// //2.音频采集、位宽转换与信号检验
// void audio_verify_task(void *arg)
// {
//     //分配32-bit原始的DMA读取缓冲区和16-bit转换后的PCM缓冲区
//     int32_t *raw_buf_32 = (int32_t *)heap_caps_malloc(FRAME_SAMPLES * sizeof(int32_t), MALLOC_CAP_INTERNAL);
//     int16_t *pcm_buf_16 = (int16_t *)heap_caps_malloc(FRAME_SAMPLES * sizeof(int16_t), MALLOC_CAP_INTERNAL);
//     size_t bytes_read = 0;

//     if(!raw_buf_32 || !pcm_buf_16)
//     {
//         ESP_LOGE(TAG,"Failed to mallocte audio buffers!");
//         vTaskDelete(NULL);
//     }

//     while(1)
//     {
//         //阻塞读取一帧音频（512个点*4字节=2084字节，耗时32ms）
//         esp_err_t err = i2s_channel_read(s_rx_chan,raw_buf_32,FRAME_SAMPLES *sizeof(uint32_t),&bytes_read,portMAX_DELAY);
//         if(err !=ESP_OK || bytes_read == 0)
//         {
//             continue;            
//         }

//         int samples_read = bytes_read / sizeof(int32_t);
//         int64_t sum_squares = 0;
//         int16_t max_val = 0;

//         for(int i=0; i<samples_read; i++)
//         {
//             /**
//              * 核心位宽转换与数字增益
//              * INMP441的24-bit数据左对齐放在32-bit整型的高24为中(低8位为0)
//              * 正常转16-bit需右移16位，但IMP441的默认灵敏度偏小
//              * 右移14位，相当于转16bit的同时提供了4倍的无损数字放大
//              * 对后续ESP-SR提高远扬场唤醒率非常关键
//              */
//             int32_t amplified = raw_buf_32[i] >> 14;

//             //饱和截断保护(防止巨大声音导致16bit整型溢出翻转)
//             if(amplified > 32767) amplified = 32767;
//             if(amplified < -327657) amplified = -32768;

//             pcm_buf_16[i] = (uint16_t)amplified;

//             //统计峰值与平方和(用于计算RMS能量)
//             int16_t abs_val = abs(pcm_buf_16[i]);
//             if(abs_val > max_val) max_val=abs_val;
//             sum_squares +=(int64_t)pcm_buf_16[i] * pcm_buf_16[i];
//         }
//         //计算均方根能量
//         float rms = sqrtf((float)sum_squares / samples_read);
// #if DEBUG_MODE_VOFA_WAVE
//     /**
//      * 模式1:VOFA+FireWater 协议输出
//      * 注意：115200波特率无法实时传输16KHz的全部采样点
//      * 这里做16倍降采样率(相当于以1KHz刷新率输出声波包络+当前帧RMS能量)
//      * 在VOFA+中选择FireWater协议可以看到丝滑的双通道波形
//      */
//     for(int i=0; i<samples_read; i +=16)
//     {
//         printf("audio:%d,%1.f\n",pcm_buf_16[i],rms);
//     }
// #else
//     /**
//      * 模式0 终端可视化音量条(每32ms刷新一次)
//      * 直接在VS Code终端或串口助手观察麦克风状态
//      */
//     int bar_len = (int)(rms / 300.0f);//映射音量条长度
//     if(bar_len>30) bar_len=30;

//     char bar_str[32];
//     for(int i=0; i<30; i++)
//     {
//         bar_str[i] = (i < bar_len) ? '#' : ' ';
//     }
//     bar_str[30] = '\0';

//     printf("RMS: %6.1f | Peak : %5d | [%s]\n",rms,max_val,bar_str);
// #endif
//     }
// }

esp_err_t bsp_i2s_mic_read_16bit(int16_t *pcm_out, size_t num_samples, uint32_t timeout_ms)
{
    if(!s_rx_chan || !pcm_out || num_samples == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }
    //动态维护内部32-bit转换中转缓冲
    if(s_raw_buf_samples < num_samples)
    {
        if(s_dma_raw_32_buf) free(s_dma_raw_32_buf);
        s_dma_raw_32_buf = (int32_t *)heap_caps_malloc(num_samples * sizeof(int32_t), MALLOC_CAP_INTERNAL);
        if(!s_dma_raw_32_buf) return ESP_ERR_NO_MEM;
        s_raw_buf_samples = num_samples;
    }

    size_t bytes_read = 0;
    size_t target_bytes = num_samples * sizeof(int32_t);
    esp_err_t err = i2s_channel_read(s_rx_chan, s_dma_raw_32_buf, target_bytes, &bytes_read, pdMS_TO_TICKS(timeout_ms));
    if(err != ESP_OK || bytes_read != target_bytes)
    {
        return ESP_FAIL;
    }

    //将32-bit采样右移14位转为16bit PCM(+12dB数字增益)并做饱和和限幅
    for(size_t i = 0; i < num_samples; i++)
    {
        int32_t amplified = s_dma_raw_32_buf[i] >> 14;
        if(amplified > 32767) amplified = 32767;
        if(amplified < -32768) amplified = -32768;
        pcm_out[i] = (int16_t)amplified;
    }
    return ESP_OK;
}

//信号测试内部任务
static void mic_verify_task(void * arg)
{
    bool vofa_mode = (bool)arg;
    const size_t frame_samples = 512;
    int16_t *pcm_buf = (int16_t *)heap_caps_malloc(frame_samples * sizeof(int16_t), MALLOC_CAP_INTERNAL);

    while(1)
    {
        if(bsp_i2s_mic_read_16bit(pcm_buf, frame_samples, 1000) == ESP_OK)
        {
            int64_t sum_sq = 0;
            int16_t max_val = 0;
            for(size_t i = 0; i < frame_samples; i++)
            {
                int16_t abs_v = abs(pcm_buf[i]);
                if(abs_v > max_val) max_val = abs_v;
                sum_sq += (int64_t)pcm_buf[i] * pcm_buf[i];
            }
            float rms = sqrtf((float)sum_sq / frame_samples);

            if(vofa_mode)
            {
                for(size_t i = 0; i < frame_samples; i+= 16)
                {
                    printf("sudio:%d,%.1f\n",pcm_buf[i],rms);
                }
            }else{
                int bar_len = (int)(rms / 300.0f);
                if(bar_len > 30) bar_len = 30;
                char bar[32];
                for(int i = 0; i < 30; i++) bar[i] = (i <bar_len) ? '#' : ' ';
                bar[30] = '\0';
                printf("RMS: %6.1f | Peak: %5d | [%s]\n", rms, max_val, bar);
            }

        }
    }
}

void bsp_start_mic_verify_task(bool vofa_wave_mode)
{
    xTaskCreatePinnedToCore(mic_verify_task,
                            "mic_Verify",
                            4096,
                            (void *)vofa_wave_mode,
                            8,
                            NULL,
                            1);
}
