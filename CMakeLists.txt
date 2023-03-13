cmake_minimum_required(VERSION 3.20.0 FATAL_ERROR)

# Setup compiler and mcu arch 
set(COMPILER_TYPE "csky-elfabiv2")
set(MCU_ARCH_TYPE "ck804ef")

# Include toolchain config
include(${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain/${COMPILER_TYPE}/${MCU_ARCH_TYPE}.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/utils/kconfig.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/utils/utils.cmake)

# Set up the project
project("w801"
    VERSION "0.1.0"
    LANGUAGES C ASM CXX)

# Redirect static lib path
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY          ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG    ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE  ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
link_directories(${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})

# Auto add subdirectory
file(GLOB sub_files RELATIVE "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/*")
foreach(file_name ${sub_files})
    if(IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/${file_name}")
        if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/${file_name}/CMakeLists.txt")
            message(STATUS "add_subdirectory: ${CMAKE_CURRENT_LIST_DIR}/${file_name}")
            add_subdirectory("${file_name}")
        endif()
    endif()
endforeach()

include(${PROJECT_BINARY_DIR}/sdk/sdk_conf.cmake)

#
# user executable target
#

file(GLOB_RECURSE USER_C_SRCS LIST_DIRECTORIES false RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "src/*.c")

add_executable(${CMAKE_PROJECT_NAME}
    ${USER_C_SRCS}
    ${USER_ASM_SRCS})

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    )

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    "-Wno-implicit-function-declaration"
    "-Wno-unused-function")

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -nostartfiles
    -Wl,-zmax-page-size=1024
    -T "${PROJECT_SOURCE_DIR}/sdk/ld/w800/gcc_csky.ld"
    -Wl,-ckmap=${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.map
    -lm)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE w800sdk)

set_target_properties(${CMAKE_PROJECT_NAME} 
    PROPERTIES LINK_DEPENDS ${PROJECT_SOURCE_DIR}/sdk/ld/w800/gcc_csky.ld)

# this func call make __FILE__ macro use relative path for every source files
use_repath_for_macro__FILE__(${CMAKE_PROJECT_NAME})

#
# make output files
#

# setup msys
if(CMAKE_HOST_WIN32)
    file(RELATIVE_PATH MSYS_BIN_PATH_FOR_BUILD_DIR ${CMAKE_BINARY_DIR} ${PROJECT_SOURCE_DIR}/tools/msys/bin)
    message(STATUS "Insert msys setup command for win32")
    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} PRE_LINK
        COMMAND ${CMAKE_COMMAND} -E echo "check and setup msys for win32 ..."
        COMMAND ${PROJECT_SOURCE_DIR}/tools/setup.bat
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        VERBATIM)
endif()

# make firmware
configure_file("${PROJECT_SOURCE_DIR}/tools/make_firmware.sh.in" "${CMAKE_BINARY_DIR}/make_firmware.sh" @ONLY)
configure_file("${PROJECT_SOURCE_DIR}/tools/flash_firmware.sh.in" "${PROJECT_SOURCE_DIR}/bin/flash_firmware.sh" @ONLY)
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "=== make firmware ... ==="
    COMMAND ${PROJECT_SOURCE_DIR}/tools/msys/bin/bash "${CMAKE_BINARY_DIR}/make_firmware.sh"
    DEPENDS ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.elf
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    VERBATIM)
