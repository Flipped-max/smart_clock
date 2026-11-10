set(src 

            "mid_audio_sr.c")
set(include_dirs 
            
            "include"
            "../../main")
set(requires

            "driver"
            "bsp_board"
            "esp-sr"
            "freertos")


idf_component_register(
    SRCS ${src} 
    INCLUDE_DIRS ${include_dirs} 
    REQUIRES ${requires}
)

component_compile_options(
    -ffast-math 
    -O3 
    -Wno-error=format 
    -Wno-format
)
