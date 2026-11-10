#ifndef __APP_COMMON_H
#define __APP_COMMON_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

/*==========引脚分配===============*/
//INMP441
#define BSP_INMP441_PIN_SCK GPIO_NUM_41
#define BSP_INMP441_PIN_WS GPIO_NUM_42
#define BSP_INMP441_PIN_DIN GPIO_NUM_2
//LCD(ST7789)
#define BSP_SPI_PIN_CLK GPIO_NUM_12
#define BSP_SPI_PIN_MOSI GPIO_NUM_11
#define BSP_SPI_PIN_MISO -1
#define BSP_LCD_PIN_CS GPIO_NUM_10
#define BSP_LCD_PIN_DC GPIO_NUM_9
#define BSP_LCD_PIN_RST -1
#define BSP_LCD_PIN_BK -1

/*==========音频与PSRAM录音参数=========*/
#define SAMPLE_RATE 16*1000 //ESP_SR标准采样率16k
#define FRAME_SAMPLES 512 //每次读取512个采样点
#define MAX_REC_TIME_SEC 5
//16000Hz * 2Bytes(16bit) * 5s = 160,000 Bytews(160KB)
#define PSRAM_REC_BUF_SIZE (SAMPLE_RATE * sizeof(int16_t) * MAX_REC_TIME_SEC)

/*===========全局FreeROTS事件组标志位==========*/
#define WIFI_CONNECTED_BIT  (1 << 0) //WIFI连接成功
#define AUDIO_REC_DONE_BIT  (1 << 1) //PSRAM录音完成，通知云端任务读取
#define AI_PROCESS_DONE_BIT (1 << 2) //云端ASR+LLM处理完毕，允许重新开启唤醒

/*============UI消息队列数据结构================*/
typedef enum{
    UI_EVT_STATE_IDLE = 0,      //待机时钟状态
    UI_EVT_STATE_LISTENING,     //唤醒成功，正在录音
    UI_EVT_STATE_THINKING,      //录音结束，正在请求云端ASR/LLM
    UI_EVT_TEXT_CLEAR,          //清空上一次对话文本
    UI_EVT_TEXT_ASR_RESULT,     //显示用户说的话(ASR结果)
    UI_EVT_TEXT_LLM_APPEND,     //显示最佳大模型回答的增量字符
}ui_event_type_t;

typedef struct{
    ui_event_type_t type;
    char text[64];      //存放增量文本片段
}ui_msg_t;

//申明全局跨模块同步句柄(在mian中定义)
extern EventGroupHandle_t g_sys_event_group;
extern QueueHandle_t      g_ui_msg_queue;

//声明全局共享PSRAM录音缓冲区与有效字节数
extern int16_t *g_psram_res_buf;
extern size_t   g_rec_bytes;


#endif