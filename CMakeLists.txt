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
# For other generators (Ninja, NMake, etc.), detect platform using CMAKE_GENERATOR_PLATFORM
# or fall back to CMAKE_SYSTEM_PROCESSOR
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
elseif(DEFINED CMAKE_GENERATOR_PLATFORM AND NOT CMAKE_GENERATOR_PLATFORM STREQUAL "")
    # For generators that support -A flag but don't set CMAKE_VS_PLATFORM_NAME
    if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
        set(VLD_PLATFORM "ARM64")
        set(VLD_PLATFORM_DEFINE "WIN64")
    elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
        set(VLD_PLATFORM "x64")
        set(VLD_PLATFORM_DEFINE "WIN64")
    else()
        set(VLD_PLATFORM "Win32")
        set(VLD_PLATFORM_DEFINE "WIN32")
    endif()
else()
    # Fallback for Ninja, NMake, etc. - detect from compiler/system
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(VLD_PLATFORM "ARM64")
        set(VLD_PLATFORM_DEFINE "WIN64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(VLD_PLATFORM "x64")
        set(VLD_PLATFORM_DEFINE "WIN64")
    else()
        set(VLD_PLATFORM "Win32")
        set(VLD_PLATFORM_DEFINE "WIN32")
    endif()
endif()

# MSVC specific settings
if(MSVC)
    # Default to DLL runtime for tests (they need it for GetProcAddress tests)
    # VLD core library overrides this to use static runtime
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    
    # Common compile options - use highest warning level and treat warnings as errors
    add_compile_options(/W4 /WX)
    add_link_options(/WX)
    
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

# Third-party libraries: don't treat warnings as errors and use static runtime
if(MSVC)
    target_compile_options(fmt PRIVATE /W3 /WX-)
    set_property(TARGET fmt PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# IDE folder organization
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_target_properties(gtest PROPERTIES FOLDER "Libraries")
set_target_properties(fmt PROPERTIES FOLDER "Libraries")
set_target_properties(vld PROPERTIES FOLDER "VLD")
