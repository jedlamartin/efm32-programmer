idf_component_register(
    SRCS "src/swd_link.cpp"
    INCLUDE_DIRS "include" "${CMAKE_SOURCE_DIR}/include"
    REQUIRES esp_driver_gpio
)