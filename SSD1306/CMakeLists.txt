# SSD1306/CMakeLists.txt

add_library(SSD1306 STATIC)

target_sources(SSD1306 PRIVATE
    Src/ssd1306.c
)

target_include_directories(SSD1306 PUBLIC
    Inc
)

target_link_libraries(SSD1306 PRIVATE stm32cubemx)
