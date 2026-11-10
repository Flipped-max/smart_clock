idf_component_register(
    SRCS "app_ui.c"
    INCLUDE_DIRS "include" "../../main"
    REQUIRES bsp_board 
    lvgl__lvgl 
    espressif__esp_lvgl_port
    littlefs
)
