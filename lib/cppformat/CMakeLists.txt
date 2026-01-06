# libformat - C++ formatting library
cmake_minimum_required(VERSION 3.16)

add_library(libformat STATIC
    format.cc
    format.h
    posix.cc
    posix.h
)

target_include_directories(libformat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# MSVC settings
if(MSVC)
    # Use static runtime to match VLD core
    set_property(TARGET libformat PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(libformat PRIVATE /W3)
    target_compile_definitions(libformat PRIVATE
        _CRT_SECURE_NO_WARNINGS
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
    )
endif()

# Set output name
set_target_properties(libformat PROPERTIES
    OUTPUT_NAME "libformat"
    DEBUG_POSTFIX ""
)
