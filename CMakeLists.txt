cmake_minimum_required(VERSION 3.22)
project(ECHO LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# New .c/.cpp files under src/ are picked up on the next build.
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_executable(ECHO ${APP_SOURCES})
target_include_directories(ECHO PRIVATE include)

# Keep using CubeMX's source list, including any newly enabled peripherals.
include(cmake/check-app-hooks.cmake)
add_subdirectory(ECHO/cmake/stm32cubemx)
target_link_libraries(ECHO stm32cubemx)

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/ECHO/STM32G431xx_FLASH.ld")
target_link_options(ECHO PRIVATE
    "-T${LINKER_SCRIPT}"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/ECHO.map"
    -Wl,--gc-sections
    -Wl,--print-memory-usage)
set_property(TARGET ECHO APPEND PROPERTY LINK_DEPENDS "${LINKER_SCRIPT}")
set_target_properties(ECHO PROPERTIES SUFFIX ".elf")

add_custom_command(TARGET ECHO POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:ECHO>" ECHO.hex
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:ECHO>" ECHO.bin
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:ECHO>"
    BYPRODUCTS ECHO.hex ECHO.bin
    COMMENT "Creating firmware HEX and BIN files"
    VERBATIM)
