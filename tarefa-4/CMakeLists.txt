# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
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

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(tarefa-4 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable
add_executable(tarefa-4 tarefa-4.c ws2818b.pio)

pico_set_program_name(tarefa-4 "tarefa-4")
pico_set_program_version(tarefa-4 "0.1")

# Generate PIO header (use the .program name from the .pio file, not the executable name!)
pico_generate_pio_header(tarefa-4 ${CMAKE_CURRENT_LIST_DIR}/ws2818b.pio)

# Enable UART and/or USB
pico_enable_stdio_uart(tarefa-4 1)
pico_enable_stdio_usb(tarefa-4 0)

# Link libraries
target_link_libraries(tarefa-4
        pico_stdlib
        hardware_i2c
        hardware_pio
        hardware_clocks
)

# Include current directory
target_include_directories(tarefa-4 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add extra outputs
pico_add_extra_outputs(tarefa-4)
