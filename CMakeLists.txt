# ==============================================================================
# Iec60870Lib - Embedded lib60870 target (STM32 + FreeRTOS)
#
# We intentionally do not use upstream host-focused CMake.
# This target builds a focused CS101 stack with STM32 HAL adapters.
# ==============================================================================

cmake_minimum_required(VERSION 3.16)

set(LIB60870_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(LIB60870_SRC  ${LIB60870_ROOT}/src)

option(LIB60870_USE_THREADS "Enable internal lib60870 worker threads" ON)
set(LIB60870_FILE_SERVICE_SRC ${LIB60870_SRC}/file-service/file_server.c)
option(LIB60870_ENABLE_FILE_SERVICE "Enable CS101 file service plugin source" ON)

set(LIB60870_CONFIG_USE_THREADS 0)
set(LIB60870_CONFIG_USE_SEMAPHORES 0)
if(LIB60870_USE_THREADS)
    set(LIB60870_CONFIG_USE_THREADS 1)
    set(LIB60870_CONFIG_USE_SEMAPHORES 1)
endif()

set(IEC60870_CORE_SRCS
    ${LIB60870_SRC}/iec60870/apl/cpXXtime2a.c
    ${LIB60870_SRC}/iec60870/cs101/cs101_asdu.c
    ${LIB60870_SRC}/iec60870/cs101/cs101_information_objects.c
    ${LIB60870_SRC}/iec60870/cs101/cs101_master_connection.c
    ${LIB60870_SRC}/iec60870/cs101/cs101_queue.c
    ${LIB60870_SRC}/iec60870/cs101/cs101_slave.c
    ${LIB60870_SRC}/iec60870/link_layer/buffer_frame.c
    ${LIB60870_SRC}/iec60870/link_layer/link_layer.c
    ${LIB60870_SRC}/iec60870/link_layer/serial_transceiver_ft_1_2.c
    ${LIB60870_SRC}/iec60870/frame.c
    ${LIB60870_SRC}/iec60870/lib60870_common.c
)

set(IEC60870_COMMON_SRCS
    ${LIB60870_SRC}/common/linked_list.c
)

set(IEC60870_HAL_SRCS
    ${LIB60870_SRC}/hal/memory/lib_memory.c
    ${LIB60870_SRC}/hal/serial/stm32/serial_port_stm32.c
    ${LIB60870_SRC}/hal/time/stm32/hal_time_stm32.c
)

if(LIB60870_USE_THREADS)
    list(APPEND IEC60870_HAL_SRCS
        ${LIB60870_SRC}/hal/thread/freertos/thread_freertos.c
    )
endif()

set(IEC60870_FILE_SERVICE_SRCS)
if(LIB60870_ENABLE_FILE_SERVICE AND EXISTS ${LIB60870_FILE_SERVICE_SRC})
    list(APPEND IEC60870_FILE_SERVICE_SRCS
        ${LIB60870_FILE_SERVICE_SRC}
    )
endif()

add_library(Iec60870Lib STATIC
    ${IEC60870_CORE_SRCS}
    ${IEC60870_COMMON_SRCS}
    ${IEC60870_HAL_SRCS}
    ${IEC60870_FILE_SERVICE_SRCS}
)

target_include_directories(Iec60870Lib
    PUBLIC
        ${LIB60870_SRC}/inc/api
        ${LIB60870_SRC}/hal/inc
        ${LIB60870_SRC}/hal/serial/stm32
        ${CMAKE_SOURCE_DIR}/Core/Inc
    PRIVATE
        ${LIB60870_SRC}/inc/internal
        ${LIB60870_SRC}/common/inc
)

if(LIB60870_ENABLE_FILE_SERVICE AND EXISTS ${LIB60870_FILE_SERVICE_SRC})
    target_include_directories(Iec60870Lib PUBLIC ${LIB60870_SRC}/file-service)
endif()

target_compile_definitions(Iec60870Lib
    PUBLIC
        CONFIG_USE_THREADS=${LIB60870_CONFIG_USE_THREADS}
        CONFIG_USE_SEMAPHORES=${LIB60870_CONFIG_USE_SEMAPHORES}
        CONFIG_DEBUG_OUTPUT=0
)

target_link_libraries(Iec60870Lib
    PUBLIC
        freertos_kernel
        freertos_config
        STM32F4HAL
)
