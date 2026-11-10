#ifndef __INMP441_H
#define __INMP441_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"


// 调试模式切换：
// 设为 0：在普通串口终端打印直观的【音量进度条 + 峰值 + RMS】
// 设为 1：输出 FireWater 格式数据供【VOFA+】软件绘制实时声波曲线
//#define DEBUG_MODE_VOFA_WAVE 0

esp_err_t bsp_i2s_mic_init(void);

esp_err_t bsp_i2s_mic_read_16bit(int16_t *pcm_out, size_t num_samples, uint32_t timeout_ms);

void bsp_start_mic_verify_task(bool vofa_wave_mode);

#endif


