#ifndef __USART_H_
#define __USART_H_

#include "driver/uart.h"
#include "driver/uart_select.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

#define USART_NUM UART_NUM_0
#define USART_TX_PIN GPIO_NUM_43
#define USART_RX_PIN GPIO_NUM_44

#define RX_BUF_SIZE 1024

void usart_init(uint32_t baudrate);

#endif


