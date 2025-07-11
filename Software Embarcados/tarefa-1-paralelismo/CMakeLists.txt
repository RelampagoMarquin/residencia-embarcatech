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
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(tarefa-1-paralelismo C CXX ASM)

# Import do FreeRTOS
include(${CMAKE_CURRENT_LIST_DIR}/FreeRTOSv202406.01-LTS/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)


# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(tarefa-1-paralelismo tarefa-1-paralelismo.c inc/ssd1306_i2c.c ${CMAKE_CURRENT_LIST_DIR}/FreeRTOSv202406.01-LTS/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/MemMang/heap_4.c
) 

pico_set_program_name(tarefa-1-paralelismo "tarefa-1-paralelismo")
pico_set_program_version(tarefa-1-paralelismo "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(tarefa-1-paralelismo 1)
pico_enable_stdio_usb(tarefa-1-paralelismo 0)

# Add the standard library to the build
target_link_libraries(tarefa-1-paralelismo
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(tarefa-1-paralelismo PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Adiciona definições de compilação para o FreeRTOS.
# Use PUBLIC para garantir que as definições se propaguem corretamente.
target_compile_definitions(tarefa-1-paralelismo PUBLIC
    # O número de núcleos que o FreeRTOS deve gerenciar. 1 para Pico, 2 se usar SMP.
    configNUMBER_OF_CORES=1
)

# Adiciona o diretório do projeto aos includes para encontrar seus headers locais
target_include_directories(tarefa-1-paralelismo PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(tarefa-1-paralelismo
        pico_stdlib
        hardware_spi
        hardware_i2c
        hardware_pio
        hardware_adc
        FreeRTOS-Kernel
        
        )

pico_add_extra_outputs(tarefa-1-paralelismo)

