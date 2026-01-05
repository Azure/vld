cmake_minimum_required(VERSION 3.16)

project(VLD
    VERSION 2.5.10
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
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(VLD_PLATFORM "x64")
    set(VLD_PLATFORM_DEFINE "WIN64")
else()
    set(VLD_PLATFORM "Win32")
    set(VLD_PLATFORM_DEFINE "WIN32")
endif()

# MSVC specific settings
if(MSVC)
    # Use static runtime library for non-MFC builds
    # MFC targets will override this to use DLL runtime
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Set platform toolset
    set(CMAKE_VS_PLATFORM_TOOLSET "v142" CACHE STRING "Platform toolset")
    
    # Common compile options
    add_compile_options(/W3)
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

# Google Test configuration
set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
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
