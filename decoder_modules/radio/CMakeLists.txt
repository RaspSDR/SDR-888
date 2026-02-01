cmake_minimum_required(VERSION 3.13)
project(radio)

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

# MSVC can't compile C99 complex; compile the CDRDemod C sources as C++ so VOLK uses std::complex.
if(MSVC)
    file(GLOB_RECURSE CDRDEMOD_C "src/cdr/CDRDemod/src/*.c")
    set_source_files_properties(${CDRDEMOD_C} PROPERTIES LANGUAGE CXX)
endif()
    
include(${SDRPP_MODULE_CMAKE})

target_compile_definitions(radio PRIVATE HAVE_LIBFDK_AAC HAVE_USAC HAVE_LIBZ)
target_include_directories(radio PRIVATE "src/")
target_include_directories(radio PRIVATE "src/drm/")
target_include_directories(radio PRIVATE "src/cdr/include")

if(MSVC)
    find_package(fdk-aac CONFIG REQUIRED)
    find_package(ZLIB REQUIRED)

    target_link_libraries(radio PRIVATE FDK-AAC::fdk-aac)
    target_link_libraries(radio PRIVATE ZLIB::ZLIB)
else()
    find_package(PkgConfig)

    pkg_check_modules(FDKAAC REQUIRED fdk-aac)
    pkg_check_modules(ZLIB REQUIRED zlib)

    target_include_directories(sdrpp_core PUBLIC
        ${FDKAAC_INCLUDE_DIRS}
        ${ZLIB_INCLUDE_DIRS}
    )

    target_link_directories(sdrpp_core PUBLIC
        ${FDKAAC_LIBRARY_DIRS}
        ${ZLIB_LIBRARY_DIRS}
    )

    target_link_libraries(sdrpp_core PUBLIC
        ${FDKAAC_LIBRARIES}
        ${ZLIB_LIBRARIES}
    )
endif()
