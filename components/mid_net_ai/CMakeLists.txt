set(requires 

            driver
            nvs_flash 
            esp_wifi 
            esp_event 
            esp_http_client 
            esp-tls 
            json)

idf_component_register(
    SRCS "mid_net_ai.c"
    INCLUDE_DIRS "include" "../../main"
    REQUIRES ${requires}
    )