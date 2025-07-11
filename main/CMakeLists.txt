set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h")

idf_component_register(
  SRCS "main.c" "storage_manager.c"
  INCLUDE_DIRS ""
  REQUIRES lvgl
)
