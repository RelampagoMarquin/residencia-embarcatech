# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if(EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/FreeRTOSv202406.01-LTS/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(tarefa-2-iot C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(tarefa-2-iot tarefa-2-iot.c inc/ssd1306_i2c.c)

pico_set_program_name(tarefa-2-iot "tarefa-2-iot")
pico_set_program_version(tarefa-2-iot "0.1")

# Generate PIO header
pico_generate_pio_header(tarefa-2-iot ${CMAKE_CURRENT_LIST_DIR}/blink.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(tarefa-2-iot 0)
pico_enable_stdio_usb(tarefa-2-iot 0)

target_compile_definitions(tarefa-2-iot PRIVATE
    configNUMBER_OF_CORES=1
)

# Add the standard library to the build
target_link_libraries(tarefa-2-iot
    pico_stdlib
    FreeRTOS-Kernel-Heap4
)

# Add the standard include files to the build
target_include_directories(tarefa-2-iot PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/lib
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${CMAKE_CURRENT_LIST_DIR}/config
    ${PICO_SDK_PATH}/src/rp2_common/pico_mbedtls/include
)

# Add any user requested libraries
target_link_libraries(tarefa-2-iot
    pico_stdlib
    hardware_gpio
    hardware_i2c
    hardware_pwm
    hardware_pio
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mqtt
    hardware_adc
)

pico_add_extra_outputs(tarefa-2-iot)

