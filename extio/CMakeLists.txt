cmake_minimum_required(VERSION 3.13)
project(ExtIO_SDDC)

if (NOT WIN32)
    message(STATUS "ExtIO build is Windows-only")
    return()
endif()

file(GLOB SRC "*.cpp" "extio.def" "resource.rc" "manifest.xml" "../source_modules/sddc_source/src/pffft/pf_mixer.cpp")

add_library(ExtIO_SDDC SHARED ${SRC})
set_target_properties(ExtIO_SDDC PROPERTIES PREFIX "")

set(SDRPP_CORE_ROOT "${CMAKE_SOURCE_DIR}/core/src")

target_include_directories(ExtIO_SDDC PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SDRPP_CORE_ROOT}"
    "${CMAKE_SOURCE_DIR}/source_modules/sddc_source/src"
    "${CMAKE_SOURCE_DIR}/source_modules/sddc_source/src/pffft"
)

target_compile_definitions(ExtIO_SDDC PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)

# sddc library (from sddc_source module fetch or system install)
set(SDDC_ROOT "${CMAKE_BINARY_DIR}/_deps/sddc-src")
if (EXISTS "${SDDC_ROOT}/include/libsddc.h")
    add_library(sddc_extio SHARED IMPORTED GLOBAL)
    set_target_properties(sddc_extio PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SDDC_ROOT}/include"
        IMPORTED_IMPLIB "${SDDC_ROOT}/lib/sddc.dll.lib"
        IMPORTED_LOCATION "${SDDC_ROOT}/lib/sddc.dll"
    )
else()
    find_path(SDDC_INCLUDE_DIR libsddc.h)
    find_library(SDDC_LIBRARY sddc)
    if (NOT SDDC_INCLUDE_DIR OR NOT SDDC_LIBRARY)
        message(FATAL_ERROR "sddc dependency not found. Configure sddc_source or set SDDC_INCLUDE_DIR/SDDC_LIBRARY")
    endif()
    add_library(sddc_extio SHARED IMPORTED GLOBAL)
    set_target_properties(sddc_extio PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SDDC_INCLUDE_DIR}"
        IMPORTED_IMPLIB "${SDDC_LIBRARY}"
        IMPORTED_LOCATION "${SDDC_LIBRARY}"
    )
endif()

target_link_libraries(ExtIO_SDDC PRIVATE sddc_extio)

find_package(FFTW3f CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)

target_link_libraries(ExtIO_SDDC PRIVATE FFTW3::fftw3f volk)
