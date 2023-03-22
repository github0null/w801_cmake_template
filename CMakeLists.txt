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

# Set up w800 sdk
set(W800_SDK_DIR "sdk")
set(W800_SDK_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/w800sdk.conf")

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

include("${PROJECT_BINARY_DIR}/${W800_SDK_DIR}/w800sdk_conf.cmake")

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

configure_file("${PROJECT_SOURCE_DIR}/${W800_SDK_DIR}/ld/w800/gcc_csky.ld.in" "${PROJECT_BINARY_DIR}/gcc_csky.ld" @ONLY)
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -nostartfiles
    -Wl,-zmax-page-size=1024
    -T "${PROJECT_BINARY_DIR}/gcc_csky.ld"
    -Wl,-ckmap=${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.map
    -lm)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE w800sdk)

set(LINK_DEPS
    "${PROJECT_SOURCE_DIR}/${W800_SDK_DIR}/ld/w800/gcc_csky.ld.in"
    "${PROJECT_SOURCE_DIR}/${W800_SDK_DIR}/tools/make_firmware.sh.in")

if(DEFINED W800_SDK_CONF_PATH)
    list(APPEND LINK_DEPS ${W800_SDK_CONF_PATH})
else()
    list(APPEND LINK_DEPS ${PROJECT_SOURCE_DIR}/${W800_SDK_DIR}/sdk.conf)
endif()

list(JOIN LINK_DEPS ";" LINK_DEPS_STR)
message(STATUS "link deps: ${LINK_DEPS_STR}")
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES LINK_DEPENDS "${LINK_DEPS_STR}")

# this func call make __FILE__ macro use relative path for every source files
use_repath_for_macro__FILE__(${CMAKE_PROJECT_NAME})

#
# make output files
#

if(CMAKE_HOST_WIN32)
    set(EXE_SUFFIX ".exe")
endif()

set(FIRMWARE_OUT_DIR_NAME bin)
set(FIRMWARE_FLASHER_TOOL ${PROJECT_SOURCE_DIR}/${FIRMWARE_OUT_DIR_NAME}/wm_tool${EXE_SUFFIX})
set(FIRMWARE_BASH_SHELL   bash)

# setup msys
if(CMAKE_HOST_WIN32)
    set(WIN32_MSYS_BASH ${PROJECT_SOURCE_DIR}/${W800_SDK_DIR}/tools/msys/bin/bash.exe)
    set(FIRMWARE_BASH_SHELL ${WIN32_MSYS_BASH})
    file(RELATIVE_PATH MSYS_BIN_PATH_FOR_BUILD_DIR ${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR}/${W800_SDK_DIR}/tools/msys/bin)
    add_custom_command(OUTPUT ${WIN32_MSYS_BASH}
        COMMAND ${CMAKE_COMMAND} -E echo "Setup msys for win32 ..."
        COMMAND ${PROJECT_SOURCE_DIR}/${W800_SDK_DIR}/tools/setup_msys.bat
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${W800_SDK_DIR}
        VERBATIM)
endif()

# make firmware
set(W800_TOOLS_DIR ${W800_SDK_DIR}/tools)
configure_file("${PROJECT_SOURCE_DIR}/${W800_SDK_DIR}/tools/make_firmware.sh.in" "${PROJECT_BINARY_DIR}/make_firmware.sh" @ONLY)
add_custom_target("${CMAKE_PROJECT_NAME}.fls"
    COMMAND ${CMAKE_COMMAND} -E echo "Make firmware ..."
    COMMAND ${FIRMWARE_BASH_SHELL} "${PROJECT_BINARY_DIR}/make_firmware.sh"
    COMMAND ${CMAKE_COMMAND} -E echo "Firmware path: '${PROJECT_SOURCE_DIR}/${FIRMWARE_OUT_DIR_NAME}/${CMAKE_PROJECT_NAME}.fls'"
    BYPRODUCTS "${PROJECT_SOURCE_DIR}/${FIRMWARE_OUT_DIR_NAME}/${CMAKE_PROJECT_NAME}.fls"
    DEPENDS ${CMAKE_PROJECT_NAME} ${WIN32_MSYS_BASH} ${FIRMWARE_FLASHER_TOOL}
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    VERBATIM)

# make flasher tools
if(CMAKE_HOST_WIN32)
    add_custom_command(OUTPUT ${FIRMWARE_FLASHER_TOOL}
        COMMAND ${CMAKE_COMMAND} -E echo "Make flasher tools ..."
        COMMAND xcopy ".\\${W800_SDK_DIR}\\tools\\w800\\wm_tool.exe" ".\\${FIRMWARE_OUT_DIR_NAME}\\" /H /Y > nul
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        VERBATIM)
elseif(CMAKE_HOST_UNIX)
    add_custom_command(OUTPUT ${FIRMWARE_FLASHER_TOOL}
        COMMAND ${CMAKE_COMMAND} -E echo "Make flasher tools ..."
        COMMAND gcc -O2 -lpthread -o "${FIRMWARE_FLASHER_TOOL}" "./${W800_SDK_DIR}/tools/w800/wm_tool.c"
        COMMAND chmod +x "${FIRMWARE_FLASHER_TOOL}"
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        VERBATIM)
endif()

#
# export flasher command
#

configure_file("${PROJECT_SOURCE_DIR}/${W800_SDK_DIR}/tools/flash_firmware.sh.in" "${PROJECT_SOURCE_DIR}/${FIRMWARE_OUT_DIR_NAME}/flash_firmware.sh" @ONLY)
if(CMAKE_HOST_WIN32)
    set(w800_FLASHER_WIN32_CD_CMD "echo current work dir: '%cd%'")
    configure_file("${PROJECT_SOURCE_DIR}/${W800_SDK_DIR}/tools/flash_firmware.win32.in" "${PROJECT_SOURCE_DIR}/${FIRMWARE_OUT_DIR_NAME}/flash_firmware.cmd" @ONLY)
    set(w800_FLASHER_WIN32_CD_CMD "%~d0 && cd \"%~dp0\"")
    configure_file("${PROJECT_SOURCE_DIR}/${W800_SDK_DIR}/tools/flash_firmware.win32.in" "${PROJECT_SOURCE_DIR}/${FIRMWARE_OUT_DIR_NAME}/flash_firmware.bat" @ONLY)
endif()
