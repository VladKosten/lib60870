# ==============================================================================
# Iec60870Lib - Custom embedded CMake target for lib60870-C (CS 101 slave only)
#
# This CMakeLists does NOT use the upstream lib60870-C/CMakeLists.txt because
# that file is designed for host builds (Linux/Win32).  Instead we list only
# the CS 101 slave sources we need and provide our own STM32/FreeRTOS HAL files.
#
# CONFIG_USE_THREADS   = 0  (no OS threads – CS101_Slave_run() called from a
#                             FreeRTOS task in iec101_startup)
# CONFIG_USE_SEMAPHORES = 0  (no semaphores needed without threads)
# ==============================================================================

cmake_minimum_required(VERSION 3.16)

set(LIB60870_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/lib60870-C)
set(LIB60870_SRC  ${LIB60870_ROOT}/src)

# ------------------------------------------------------------------------------
# Source file list
# ------------------------------------------------------------------------------
set(iec60870_lib_SRCS

    # --- IEC 60870-5-101 protocol core ----------------------------------------
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

    # --- Common utility -------------------------------------------------------
    ${LIB60870_SRC}/common/linked_list.c

    # --- Platform HAL (STM32 + FreeRTOS) -------------------------------------
    ${LIB60870_SRC}/hal/memory/lib_memory.c
    ${LIB60870_SRC}/hal/serial/stm32/serial_port_stm32.c
    ${LIB60870_SRC}/hal/time/freertos/hal_time_freertos.c
)

# ------------------------------------------------------------------------------
# Static library target
# ------------------------------------------------------------------------------
add_library(Iec60870Lib STATIC ${iec60870_lib_SRCS})

# PUBLIC include paths – available to consumers (e.g. iec101_startup)
target_include_directories(Iec60870Lib
    PUBLIC
        ${LIB60870_SRC}/inc/api          # cs101_slave.h, cs101_information_objects.h, …
        ${LIB60870_SRC}/hal/inc          # hal_serial.h, hal_time.h, lib_memory.h
        ${LIB60870_SRC}/hal/serial/stm32 # hal_serial_stm32.h (STM32 UART extension)
        ${LIB60870_ROOT}/config          # lib60870_config.h

    PRIVATE
        ${LIB60870_SRC}/inc/internal     # internal headers used only by the library sources
        ${LIB60870_SRC}/common/inc       # linked_list.h
)

# ------------------------------------------------------------------------------
# Compile definitions
# ------------------------------------------------------------------------------
target_compile_definitions(Iec60870Lib
    PUBLIC
        CONFIG_USE_THREADS=0             # disable thread support – use CS101_Slave_run()
        CONFIG_USE_SEMAPHORES=0          # disable semaphore support (not needed without threads)
        CONFIG_DEBUG_OUTPUT=0            # disable printf debug output
)

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------
target_link_libraries(Iec60870Lib
    PUBLIC
        freertos_kernel
        freertos_config
        STM32F4HAL
)
