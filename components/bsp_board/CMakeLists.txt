set(src_dirs 
            
            "USART"
            "INMP441"
            "SPI"
            "LCD")
set(include_dirs 
            
            "USART"
            "INMP441"
            "SPI"
            "LCD"
            "../../main")
set(requires 
            "driver"
            "esp_lcd"
            "freertos")


idf_component_register(
    SRC_DIRS ${src_dirs} 
    INCLUDE_DIRS ${include_dirs} 
    REQUIRES ${requires}
)

component_compile_options(
    -ffast-math 
    -O3 
    -Wno-error=format 
    -Wno-format
)

