idf_component_register(
    SRCS "main.c" "i2c_bus_mgr.c" "lvgl_port.c" "storage_manager.c" "waveshare_rgb_lcd_port.c"
    INCLUDE_DIRS ".")

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)

add_compile_options(-DCONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_GT911=0)
