# VLD - Visual Leak Detector DLL
cmake_minimum_required(VERSION 3.16)

# Source files
set(VLD_SOURCES
    callstack.cpp
    dllspatches.cpp
    ntapi.cpp
    stdafx.cpp
    utility.cpp
    vld.cpp
    vldapi.cpp
    vldheap.cpp
    vld_hooks.cpp
)

set(VLD_HEADERS
    callstack.h
    criticalsection.h
    crtmfcpatch.h
    dbghelp.h
    loaderlock.h
    map.h
    ntapi.h
    resource.h
    set.h
    stdafx.h
    tree.h
    utility.h
    vld.h
    vld_def.h
    vldallocator.h
    vldheap.h
    vldint.h
)

# Resource file
set(VLD_RESOURCES vld.rc)

# Create DLL
add_library(vld SHARED
    ${VLD_SOURCES}
    ${VLD_HEADERS}
    ${VLD_RESOURCES}
)

# Platform-specific output name
if(VLD_PLATFORM STREQUAL "ARM64")
    set_target_properties(vld PROPERTIES OUTPUT_NAME "vld_arm64")
    set(VLD_MANIFEST vld.dll.dependency.arm64.manifest)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(vld PROPERTIES OUTPUT_NAME "vld_x64")
    set(VLD_MANIFEST vld.dll.dependency.x64.manifest)
else()
    set_target_properties(vld PROPERTIES OUTPUT_NAME "vld_x86")
    set(VLD_MANIFEST vld.dll.dependency.x86.manifest)
endif()

# Include directories - dbghelp must come first to use our version
target_include_directories(vld PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/dbghelp/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lib/cppformat
    ${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_SOURCE_DIR}/setup
)

# Link libraries
target_link_libraries(vld PRIVATE libformat)

# DbgHelp library path
if(VLD_PLATFORM STREQUAL "ARM64")
    # ARM64: Use Windows SDK dbghelp instead of custom library
    message(STATUS "ARM64 build: Using Windows SDK dbghelp")
    target_link_libraries(vld PRIVATE dbghelp)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_link_directories(vld PRIVATE ${CMAKE_SOURCE_DIR}/lib/dbghelp/lib/x64)
    target_link_libraries(vld PRIVATE dbghelp)
else()
    target_link_directories(vld PRIVATE ${CMAKE_SOURCE_DIR}/lib/dbghelp/lib/Win32)
    target_link_libraries(vld PRIVATE dbghelp)
endif()

# Compile definitions
target_compile_definitions(vld PRIVATE
    VLD_EXPORTS
    _USRDLL
    UNICODE
    _UNICODE
    ${VLD_PLATFORM_DEFINE}
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

# MSVC specific settings
if(MSVC)
    # VLD core uses static runtime (different from tests which use DLL runtime)
    set_property(TARGET vld PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Precompiled header
    target_precompile_headers(vld PRIVATE stdafx.h)
    
    # Compiler options - disable inlining in Release/RelWithDebInfo to avoid issues with
    # __forceinline functions that have their body in .cpp files (bug in source)
    target_compile_options(vld PRIVATE
        $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:/Ob0>  # Disable inlining
    )
    
    # Linker settings
    if(VLD_PLATFORM STREQUAL "ARM64")
        # ARM64 requires base address >= 4GB
        target_link_options(vld PRIVATE
            /SUBSYSTEM:WINDOWS
            /BASE:0x100000000
        )
    else()
        target_link_options(vld PRIVATE
            /SUBSYSTEM:WINDOWS
            /BASE:0x03200000
        )
    endif()
    
    # Additional Windows libraries
    target_link_libraries(vld PRIVATE
        kernel32
        user32
        advapi32
        onecore
    )
endif()

# Copy import library and ini file after build
add_custom_command(TARGET vld POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying vld import library"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_LINKER_FILE:vld>"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/"
    COMMAND ${CMAKE_COMMAND} -E echo "Copying vld.ini"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/vld.ini"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/vld.ini"
)
