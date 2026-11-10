#include"usart.h"

void usart_init(uint32_t baudrate)
{
    uart_config_t uart_config_struct={0};
    uart_config_struct.baud_rate=baudrate;
    uart_config_struct.data_bits=UART_DATA_8_BITS;
    uart_config_struct.parity=UART_PARITY_DISABLE;
    uart_config_struct.stop_bits=UART_STOP_BITS_1;
    uart_config_struct.flow_ctrl=UART_HW_FLOWCTRL_DISABLE;
    uart_config_struct.rx_flow_ctrl_thresh=122;
    uart_config_struct.source_clk=UART_SCLK_APB;
    uart_param_config(USART_NUM,&uart_config_struct);

    //配置uart引脚
    uart_set_pin(USART_NUM,USART_TX_PIN,USART_RX_PIN,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
    //uart驱动安装
    uart_driver_install(USART_NUM,RX_BUF_SIZE*2,RX_BUF_SIZE*2,20,NULL,0);
}

