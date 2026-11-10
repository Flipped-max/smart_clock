set(requires 
            
            "driver"
            "bsp_board"
            "mid_audio_sr"
            "app_ui"
            "mid_net_ai")

idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES ${requires})

littlefs_create_partition_image(storage ../fs_data FLASH_IN_PROJECT)