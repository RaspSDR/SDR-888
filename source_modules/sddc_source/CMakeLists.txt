cmake_minimum_required(VERSION 3.13)
project(sddc_source)

file(GLOB SRC "src/*.cpp" "src/pffft/*.cpp")

include(${SDRPP_MODULE_CMAKE})

function(sddc_detect_platform out_var)
    if (WIN32)
        if (CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(${out_var} x86_64-pc-windows-msvc PARENT_SCOPE)
        else()
            set(${out_var} i686-pc-windows-msvc PARENT_SCOPE)
        endif()

    elseif (APPLE)
        if (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
            set(${out_var} aarch64-apple-darwin PARENT_SCOPE)
        else()
            set(${out_var} x86_64-apple-darwin PARENT_SCOPE)
        endif()

    elseif (UNIX)
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
            set(${out_var} aarch64-unknown-linux-gnu PARENT_SCOPE)
        else()
            set(${out_var} x86_64-unknown-linux-gnu PARENT_SCOPE)
        endif()

    else()
        message(FATAL_ERROR "Unsupported platform")
    endif()
endfunction()


sddc_detect_platform(SDDC_PLATFORM)
set(SDDC_VERSION 0.1.3)
if (WIN32)
    set(SDDC_ARCHIVE
        "sddc-${SDDC_VERSION}-${SDDC_PLATFORM}.zip"
    )
else (WIN32)
    set(SDDC_ARCHIVE
        "sddc-${SDDC_VERSION}-${SDDC_PLATFORM}.tar.gz"
    )
endif()

include(FetchContent)

Message("Download https://github.com/RaspSDR/rx888/releases/download/v${SDDC_VERSION}/${SDDC_ARCHIVE}")
FetchContent_Declare(
    SDDC
    URL https://github.com/RaspSDR/rx888/releases/download/v${SDDC_VERSION}/${SDDC_ARCHIVE}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(SDDC)
set(SDDC_ROOT "${sddc_SOURCE_DIR}")

add_library(sddc::sddc SHARED IMPORTED GLOBAL)

set_target_properties(sddc::sddc PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${SDDC_ROOT}/include"
)

if (WIN32)
    set_target_properties(sddc::sddc PROPERTIES
        IMPORTED_IMPLIB "${SDDC_ROOT}/lib/sddc.dll.lib"
        IMPORTED_LOCATION "${SDDC_ROOT}/lib/sddc.dll"
    )
elseif (APPLE)
    set_target_properties(sddc::sddc PROPERTIES
        IMPORTED_LOCATION "${SDDC_ROOT}/lib/libsddc.dylib"
    )
else() # Linux
    set_target_properties(sddc::sddc PROPERTIES
        IMPORTED_LOCATION "${SDDC_ROOT}/lib/libsddc.so"
    )
endif()

target_compile_definitions(sddc_source PRIVATE PFFFT_ENABLE_NEON)
target_link_libraries(sddc_source PUBLIC sddc::sddc)

add_custom_command(TARGET sddc_source POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:sddc::sddc>"
        "$<TARGET_FILE_DIR:sddc_source>"
)