#ifndef __MID_NET_AI_H
#define __MID_NET_AI_H

#include "esp_err.h"

/*===============WiFi设置================*/
#define WIFI_STA_SSID       "oneplus1"
#define WIFI_STA_PASSWORD   "s1234567"

/*==============Open_Meteo免费天气========*/
#define OPEN_METEO_CITY     "太原"
#define OPEN_METEO_URL      "https://api.open-meteo.com/v1/forecast?latitude=37.87&longitude=112.56&current=temperature_2m,weather_code,wind_speed_10m"

esp_err_t mid_net_ai_init_and_start(void);

#endif

