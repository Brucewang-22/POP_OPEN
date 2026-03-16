##
# @file CMakeLists.txt
# @brief 
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRCS
aux_source_directory(${APP_PATH}/src APP_SRCS)

# hardware/imu (MPU6050 + Mahony AHRS)
if (CONFIG_ENABLE_HARDWARE_IMU STREQUAL "y")
    aux_source_directory(${APP_PATH}/src/hardware/imu IMU_SRCS)
    list(APPEND APP_SRCS ${IMU_SRCS})
endif()

# hardware/lcd (GC9D01 dual independent control)
if (CONFIG_ENABLE_HARDWARE_LCD STREQUAL "y")
    aux_source_directory(${APP_PATH}/src/hardware/lcd LCD_SRCS)
    list(APPEND APP_SRCS ${LCD_SRCS})
endif()

# hardware/moto (servo trajectory + boot home)
if (CONFIG_ENABLE_HARDWARE_MOTO STREQUAL "y")
    aux_source_directory(${APP_PATH}/src/hardware/moto MOTO_SRCS)
    list(APPEND APP_SRCS ${MOTO_SRCS})
endif()

# hardware/microphone (bring-up stats monitor)
if (CONFIG_ENABLE_HARDWARE_MICROPHONE STREQUAL "y")
    aux_source_directory(${APP_PATH}/src/hardware/microphone MIC_SRCS)
    list(APPEND APP_SRCS ${MIC_SRCS})
endif()

# hardware/touch (external touch switch monitor)
if (CONFIG_ENABLE_HARDWARE_TOUCH STREQUAL "y")
    aux_source_directory(${APP_PATH}/src/hardware/touch TOUCH_SRCS)
    list(APPEND APP_SRCS ${TOUCH_SRCS})
endif()


set(APP_INC
            ${APP_PATH}/include
)

# Board public headers (e.g. board_com_api.h)
if (CONFIG_BOARD_CHOICE_LIBTECH_POP_T5AI_BOARD STREQUAL "y")
    list(APPEND APP_INC ${CMAKE_SOURCE_DIR}/boards/T5AI/LIBTECH_POP_T5AI_BOARD)
endif()

if (CONFIG_ENABLE_HARDWARE_IMU STREQUAL "y")
    list(APPEND APP_INC ${APP_PATH}/src/hardware/imu)
endif()

if (CONFIG_ENABLE_HARDWARE_LCD STREQUAL "y")
    list(APPEND APP_INC ${APP_PATH}/src/hardware/lcd)
endif()

if (CONFIG_ENABLE_HARDWARE_MOTO STREQUAL "y")
    list(APPEND APP_INC ${APP_PATH}/src/hardware/moto)
endif()

if (CONFIG_ENABLE_HARDWARE_MICROPHONE STREQUAL "y")
    list(APPEND APP_INC ${APP_PATH}/src/hardware/microphone)
endif()

if (CONFIG_ENABLE_HARDWARE_TOUCH STREQUAL "y")
    list(APPEND APP_INC ${APP_PATH}/src/hardware/touch)
endif()

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
    )

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        "-DLV_LVGL_H_INCLUDE_SIMPLE"
    )

########################################
# Add subdirectory
########################################
add_subdirectory(${APP_PATH}/../ai_components)
