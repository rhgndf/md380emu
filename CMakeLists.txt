cmake_minimum_required(VERSION 3.12)
include(FetchContent)
project(md380emu)

set(CMAKE_CXX_STANDARD 20)

# Check if unzip and python3 are installed
find_program(UNZIP_COMMAND unzip)
if(NOT UNZIP_COMMAND)
    message(FATAL_ERROR "unzip not found")
endif()

find_program(PYTHON3_COMMAND python3)
if(NOT PYTHON3_COMMAND)
    message(FATAL_ERROR "python3 not found")
endif()

# Run download.sh in firmware directory, exit if it fails
execute_process(
    COMMAND sh download.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/firmware)
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/firmware/D002.032.bin)
    message(FATAL_ERROR "download.sh failed")
endif()

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/firmware/firmware.c
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/firmware
    COMMAND xxd -i -n firmware D002.032.bin  > firmware.c
    COMMENT "Compiling firmware:D002.032.bin to binary")

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/firmware/sram.c
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/firmware
    COMMAND xxd -i -n sram d02032-core.img > sram.c
    COMMENT "Compiling sram:d02032-core.img to binary")

add_custom_target(firmware DEPENDS firmware/firmware.c firmware/sram.c)

set(DYNARMIC_TESTS OFF CACHE BOOL "")
set(DYNARMIC_USE_ARMV7 ON CACHE BOOL "")
FetchContent_Declare(
    dynarmic
    GIT_REPOSITORY https://github.com/yuzu-mirror/dynarmic.git
    GIT_TAG master
)
FetchContent_MakeAvailable(dynarmic)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/firmware/firmware.c
                            PROPERTIES GENERATED TRUE)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/firmware/sram.c
                            PROPERTIES GENERATED TRUE)

add_executable(md380emu main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/firmware/firmware.c ${CMAKE_CURRENT_SOURCE_DIR}/firmware/sram.c)
target_link_libraries(md380emu PRIVATE dynarmic)