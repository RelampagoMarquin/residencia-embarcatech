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
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(tarefa-4 C CXX ASM)

add_compile_definitions(LWIP_ALTCP_TLS=1)
add_compile_definitions(LWIP_ALTCP_TLS_MBEDTLS=1)
add_compile_definitions(LWIP_ALTCP=1)
add_compile_definitions(LWIP_ALTCP_TLS_MBEDTLS_USE_MBEDTLS=1)

include(${CMAKE_CURRENT_LIST_DIR}/FreeRTOSv202406.01-LTS/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)


# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(tarefa-4 
                    tarefa-4.c
                    inc/mqtt_psk_client.c
    
                )

add_compile_definitions(PICO_FREERTOS_HEAP_SIZE=65536)
target_compile_definitions(tarefa-4 PRIVATE "portTICK_RATE_MS=portTICK_PERIOD_MS")


pico_set_program_name(tarefa-4 "tarefa-4")
pico_set_program_version(tarefa-4 "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(tarefa-4 0)
pico_enable_stdio_usb(tarefa-4 1)

set(PICO_CYW43_ARCH_SUPPORTS_TLS 1)

# Add the standard library to the build
target_link_libraries(tarefa-4
        pico_stdlib
        #pico_cyw43_arch_lwip_threadsafe_background # Wi-Fi e pilha lwIP thread-safe
        pico_cyw43_arch_lwip_sys_freertos  # <-- ESSA LINHA É A SOLUÇÃO
        pico_lwip_mqtt      # <--- Correto
        pico_lwip_mbedtls
        pico_mbedtls            # <--- Correto
        #pico_freertos
        FreeRTOS-Kernel 
        FreeRTOS-Kernel-Heap4
        hardware_adc
        pico_bootrom
    )

# Add the standard include files to the build
target_include_directories(tarefa-4 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/inc
        ${PICO_SDK_PATH}/lib/lwip/src/include
        ${PICO_SDK_PATH}/lib/lwip/src/apps/altcp_tls
        ${PICO_SDK_PATH}/lib/mbedtls/include
        ${PICO_SDK_PATH}/lib/mbedtls/library
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOSv202406.01-LTS/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel
)
# Add any user requested libraries
target_link_libraries(tarefa-4 
        
        )

pico_add_extra_outputs(tarefa-4)

