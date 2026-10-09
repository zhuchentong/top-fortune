idf_component_register(
    SRCS "main.c"
         "fortune_model.c"
         "fortune_ui.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_hw_support esp_timer
)

# 上上签中文字库子集与签筒位图素材（生成物，见 assets/ 下脚本）。
# 只用 target_sources 这一种方式注册，避免重复编译同一资源。
target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/app_font_48.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/app_font_24.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/images/fortune_img.c"
)
