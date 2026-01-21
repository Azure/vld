cmake_minimum_required(VERSION 3.16)

project(VLD
    VERSION 2.5.11
    DESCRIPTION "Visual Leak Detector for Visual C++"
    LANGUAGES CXX C
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to enable/disable MFC support
option(VLD_USE_MFC "Build with MFC support (includes MFC test projects)" OFF)

# Option to treat warnings as errors
option(VLD_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

# Configure output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Per-configuration output directories
foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG_TYPE})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${CMAKE_BINARY_DIR}/bin/${CONFIG_TYPE})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${CMAKE_BINARY_DIR}/bin/${CONFIG_TYPE})
endforeach()

# Platform detection
# Use CMAKE_VS_PLATFORM_NAME for reliable ARM64 detection in Visual Studio generator
# NOTE: This requires Visual Studio generators. For other generators (Ninja, NMake, etc.),
#       you would need to detect platform using CMAKE_SYSTEM_PROCESSOR and CMAKE_SIZEOF_VOID_P:
#       - Check CMAKE_SYSTEM_PROCESSOR for "ARM64" or "aarch64" 
#       - Fall back to CMAKE_SIZEOF_VOID_P to distinguish x64 (8 bytes) from Win32 (4 bytes)
if(DEFINED CMAKE_VS_PLATFORM_NAME)
    if(CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64")
        set(VLD_PLATFORM "ARM64")
        set(VLD_PLATFORM_DEFINE "WIN64")
    elseif(CMAKE_VS_PLATFORM_NAME STREQUAL "x64")
        set(VLD_PLATFORM "x64")
        set(VLD_PLATFORM_DEFINE "WIN64")
    else()
        set(VLD_PLATFORM "Win32")
        set(VLD_PLATFORM_DEFINE "WIN32")
    endif()
else()
    message(FATAL_ERROR "VLD requires Visual Studio generator. CMAKE_VS_PLATFORM_NAME is not defined.")
endif()

# MSVC specific settings
if(MSVC)
    # Default to DLL runtime for tests (they need it for GetProcAddress tests)
    # VLD core library overrides this to use static runtime
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    
    # Common compile options - use highest warning level
    add_compile_options(/W4)
    
    # Treat warnings as errors if enabled
    if(VLD_WARNINGS_AS_ERRORS)
        add_compile_options(/WX)
    endif()
    
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _VARIADIC_MAX=10
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
    )
endif()

# Status messages
message(STATUS "Configuring Visual Leak Detector v${PROJECT_VERSION}")
message(STATUS "Platform: ${VLD_PLATFORM}")
message(STATUS "MFC Support: ${VLD_USE_MFC}")
message(STATUS "Warnings as Errors: ${VLD_WARNINGS_AS_ERRORS}")

# Enable testing
enable_testing()

# Google Test configuration - use DLL runtime to match tests
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

# Add subdirectories
add_subdirectory(lib/gtest)
add_subdirectory(lib/cppformat)
add_subdirectory(src)
add_subdirectory(src/tests)

# IDE folder organization
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_target_properties(gtest PROPERTIES FOLDER "Libraries")
set_target_properties(libformat PROPERTIES FOLDER "Libraries")
set_target_properties(vld PROPERTIES FOLDER "VLD")
