cmake_minimum_required(VERSION 3.26)

if (NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/arm-toolchain.cmake")
endif()

project(OmniDrive VERSION 1.0.4
        LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)

add_library(OmniDrive STATIC
        src/main.c
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/patched_firmware")

if(EXISTS "${CMAKE_SOURCE_DIR}/firmware/ASUS_BW-12B1LT_1.03.bin")
    add_library(OmniDriveMT1939 STATIC src/mt1939.c)
    # Fit the shared payload and platform adapter in the 0x1500-byte region.
    target_compile_options(OmniDriveMT1939 PRIVATE -Os)
endif()

file(GLOB firmware_list CONFIGURE_DEPENDS LIST_DIRECTORIES false "${CMAKE_SOURCE_DIR}/firmware/*.bin")
foreach(firmware_file ${firmware_list})
    cmake_path(GET firmware_file STEM LAST_ONLY firmware_name)
    set(patched_firmware_name "${firmware_name}_OmniDrive-${PROJECT_VERSION}.bin")
    set(platform_dependencies)
    if(firmware_name STREQUAL "ASUS_BW-12B1LT_1.03")
        list(APPEND platform_dependencies OmniDriveMT1939)
    else()
        list(APPEND platform_dependencies "${CMAKE_SOURCE_DIR}/asm/main.asm" "${CMAKE_SOURCE_DIR}/asm/mmio.asm")
    endif()
    add_custom_command(OUTPUT "${CMAKE_SOURCE_DIR}/patched_firmware/${patched_firmware_name}"
            COMMAND armips ${firmware_name}.asm
            COMMAND "${Python3_EXECUTABLE}" ../scripts/update_cmac.py "${CMAKE_SOURCE_DIR}/patched_firmware/${patched_firmware_name}"
            DEPENDS OmniDrive ${platform_dependencies}
                    "${firmware_file}"
                    "${CMAKE_SOURCE_DIR}/asm/${firmware_name}.asm"
                    "${CMAKE_SOURCE_DIR}/asm/version.asm"
                    "${CMAKE_SOURCE_DIR}/scripts/update_cmac.py"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/asm
            COMMENT "Patching firmware ${firmware_name}.bin"
            VERBATIM
    )
    string(MAKE_C_IDENTIFIER ${firmware_name} target_name)
    add_custom_target(${target_name} ALL 
            DEPENDS "${CMAKE_SOURCE_DIR}/patched_firmware/${patched_firmware_name}"
    )
endforeach()
