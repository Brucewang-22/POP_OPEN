##
# @file CMakeLists.txt
# @brief 
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

set(APP_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${APP_GEN_DIR})

set(HW_THRESHOLD_YAML ${APP_PATH}/src/interactive/yaml/input.yaml)
set(HW_THRESHOLD_GEN_PY ${APP_PATH}/src/interactive/python/gen_input_thresholds.py)
set(HW_THRESHOLD_GEN_H ${APP_GEN_DIR}/hardware_thresholds_config.h)
add_custom_command(
    OUTPUT ${HW_THRESHOLD_GEN_H}
    COMMAND python3 ${HW_THRESHOLD_GEN_PY} --input ${HW_THRESHOLD_YAML} --output ${HW_THRESHOLD_GEN_H}
    DEPENDS ${HW_THRESHOLD_GEN_PY} ${HW_THRESHOLD_YAML}
    COMMENT "Generating input threshold config from interactive/yaml/input.yaml"
    VERBATIM
)
add_custom_target(hardware_threshold_config_gen DEPENDS ${HW_THRESHOLD_GEN_H})

set(OUT_MODE_YAML ${APP_PATH}/src/interactive/yaml/output.yaml)
set(OUT_MODE_GEN_PY ${APP_PATH}/src/interactive/python/gen_output_modes.py)
set(OUT_MODE_GEN_H ${APP_GEN_DIR}/output_mode_config.h)
add_custom_command(
    OUTPUT ${OUT_MODE_GEN_H}
    COMMAND python3 ${OUT_MODE_GEN_PY} --input ${OUT_MODE_YAML} --output ${OUT_MODE_GEN_H}
    DEPENDS ${OUT_MODE_GEN_PY} ${OUT_MODE_YAML}
    COMMENT "Generating output mode config from interactive/yaml/output.yaml"
    VERBATIM
)
add_custom_target(output_mode_config_gen DEPENDS ${OUT_MODE_GEN_H})

# APP_SRCS
aux_source_directory(${APP_PATH}/src APP_SRCS)
aux_source_directory(${APP_PATH}/src/interactive/hal/src INTERACTIVE_SRCS)
aux_source_directory(${APP_PATH}/src/interactive/behavior/src INTERACTIVE_BEHAVIOR_SRCS)
list(APPEND INTERACTIVE_SRCS
    ${APP_PATH}/src/interactive/hal/src/hardware_abstraction.c
    ${APP_PATH}/src/interactive/hal/src/input_trigger_layer.c
)
list(REMOVE_DUPLICATES INTERACTIVE_SRCS)
list(APPEND APP_SRCS ${INTERACTIVE_SRCS})
list(APPEND APP_SRCS ${INTERACTIVE_BEHAVIOR_SRCS})

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

# hardware/audio (local speaker playback bring-up)
if (CONFIG_ENABLE_COMP_AI_AUDIO STREQUAL "y")
    aux_source_directory(${APP_PATH}/src/hardware/audio AUDIO_SRCS)
    list(APPEND APP_SRCS ${AUDIO_SRCS})
endif()

# local assets: lcd jpg frames and audio mp3 clips (embedded into firmware)
file(GLOB_RECURSE LCD_TEST_FRAME_FILES
    ${APP_PATH}/src/hardware/lcd/display_lib/*/*/*.jpg
)
set(LCD_ASSET_GEN_PY ${APP_PATH}/src/hardware/lcd/display_lib/gen_lcd_arrays.py)
set(LCD_ASSET_OUTPUTS)
set(LCD_ASSET_INC_DIRS ${APP_PATH}/src/hardware/lcd/display_lib)
foreach(LCD_FILE ${LCD_TEST_FRAME_FILES})
    get_filename_component(LCD_DIR ${LCD_FILE} DIRECTORY)
    get_filename_component(LCD_GROUP_DIR ${LCD_DIR} DIRECTORY)
    get_filename_component(LCD_GROUP_NAME ${LCD_GROUP_DIR} NAME)
    get_filename_component(LCD_SET_NAME ${LCD_DIR} NAME)
    list(APPEND LCD_ASSET_OUTPUTS
        ${LCD_DIR}/${LCD_GROUP_NAME}_${LCD_SET_NAME}_lcd_assets.c
        ${LCD_DIR}/${LCD_GROUP_NAME}_${LCD_SET_NAME}_lcd_assets.h
    )
    list(APPEND LCD_ASSET_INC_DIRS ${LCD_DIR})
endforeach()
list(REMOVE_DUPLICATES LCD_ASSET_OUTPUTS)
list(REMOVE_DUPLICATES LCD_ASSET_INC_DIRS)
add_custom_command(
    OUTPUT ${LCD_ASSET_OUTPUTS}
    COMMAND python3 ${LCD_ASSET_GEN_PY} --root ${APP_PATH}/src/hardware/lcd/display_lib
    DEPENDS ${LCD_ASSET_GEN_PY} ${LCD_TEST_FRAME_FILES}
    COMMENT "Generating embedded LCD frame assets by eye group"
    VERBATIM
)
foreach(LCD_OUTPUT ${LCD_ASSET_OUTPUTS})
    if(LCD_OUTPUT MATCHES "\\.c$")
        list(APPEND APP_SRCS ${LCD_OUTPUT})
    endif()
endforeach()

file(GLOB_RECURSE AUDIO_TEST_CLIP_FILES ${APP_PATH}/src/hardware/audio/sound_lib/*/*.mp3)
set(AUDIO_ASSET_GEN_PY ${APP_PATH}/src/hardware/audio/sound_lib/gen_audio_arrays.py)
set(AUDIO_ASSET_OUTPUTS)
set(AUDIO_ASSET_INC_DIRS)
foreach(AUDIO_FILE ${AUDIO_TEST_CLIP_FILES})
    get_filename_component(AUDIO_DIR ${AUDIO_FILE} DIRECTORY)
    get_filename_component(AUDIO_DIR_NAME ${AUDIO_DIR} NAME)
    list(APPEND AUDIO_ASSET_OUTPUTS
        ${AUDIO_DIR}/${AUDIO_DIR_NAME}_audio_assets.c
        ${AUDIO_DIR}/${AUDIO_DIR_NAME}_audio_assets.h
    )
    list(APPEND AUDIO_ASSET_INC_DIRS ${AUDIO_DIR})
endforeach()
list(REMOVE_DUPLICATES AUDIO_ASSET_OUTPUTS)
list(REMOVE_DUPLICATES AUDIO_ASSET_INC_DIRS)
add_custom_command(
    OUTPUT ${AUDIO_ASSET_OUTPUTS}
    COMMAND python3 ${AUDIO_ASSET_GEN_PY} --root ${APP_PATH}/src/hardware/audio/sound_lib
    DEPENDS ${AUDIO_ASSET_GEN_PY} ${AUDIO_TEST_CLIP_FILES}
    COMMENT "Generating embedded audio clip assets by sound_lib subdirectory"
    VERBATIM
)
foreach(AUDIO_OUTPUT ${AUDIO_ASSET_OUTPUTS})
    if(AUDIO_OUTPUT MATCHES "\\.c$")
        list(APPEND APP_SRCS ${AUDIO_OUTPUT})
    endif()
endforeach()


set(APP_INC
            ${APP_PATH}/include
            ${APP_PATH}/src/interactive/hal/include
            ${APP_PATH}/src/interactive/behavior/include
            ${APP_PATH}/src/hardware/imu
            ${APP_PATH}/src/hardware/lcd
            ${APP_PATH}/src/hardware/moto
            ${APP_PATH}/src/hardware/microphone
            ${APP_PATH}/src/hardware/touch
            ${APP_PATH}/src/hardware/audio
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

if (CONFIG_ENABLE_COMP_AI_AUDIO STREQUAL "y")
    list(APPEND APP_INC ${APP_PATH}/src/hardware/audio)
endif()

list(APPEND APP_INC ${APP_GEN_DIR})
list(APPEND APP_INC ${LCD_ASSET_INC_DIRS})
list(APPEND APP_INC ${AUDIO_ASSET_INC_DIRS})

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})
add_dependencies(${EXAMPLE_LIB} hardware_threshold_config_gen)
add_dependencies(${EXAMPLE_LIB} output_mode_config_gen)

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
