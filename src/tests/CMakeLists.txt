# VLD Test Projects
cmake_minimum_required(VERSION 3.16)

# Common include directories for all tests
set(VLD_TEST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/lib/gtest/include
)

# Common function to set up VLD test targets
function(add_vld_test TARGET_NAME)
    cmake_parse_arguments(ARG "IS_DLL;USE_MFC;HAS_PCH;NO_UNICODE" "PCH_HEADER" "SOURCES;HEADERS;DEPENDS;DEFINITIONS" ${ARGN})
    
    if(ARG_IS_DLL)
        add_library(${TARGET_NAME} SHARED ${ARG_SOURCES} ${ARG_HEADERS})
    else()
        add_executable(${TARGET_NAME} ${ARG_SOURCES} ${ARG_HEADERS})
    endif()
    
    target_include_directories(${TARGET_NAME} PRIVATE ${VLD_TEST_INCLUDE_DIRS})
    
    # Link to vld
    target_link_libraries(${TARGET_NAME} PRIVATE vld)
    
    # Additional dependencies
    if(ARG_DEPENDS)
        target_link_libraries(${TARGET_NAME} PRIVATE ${ARG_DEPENDS})
    endif()
    
    # Definitions
    target_compile_definitions(${TARGET_NAME} PRIVATE
        _VARIADIC_MAX=10
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
    )
    
    # Unicode support (most tests need it)
    if(NOT ARG_NO_UNICODE)
        target_compile_definitions(${TARGET_NAME} PRIVATE UNICODE _UNICODE)
    endif()
    
    if(ARG_DEFINITIONS)
        target_compile_definitions(${TARGET_NAME} PRIVATE ${ARG_DEFINITIONS})
    endif()
    
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_compile_definitions(${TARGET_NAME} PRIVATE WIN64)
    else()
        target_compile_definitions(${TARGET_NAME} PRIVATE WIN32)
    endif()
    
    # MFC support
    if(ARG_USE_MFC AND VLD_USE_MFC)
        set_target_properties(${TARGET_NAME} PROPERTIES
            MFC_FLAG 2  # Use MFC as a shared DLL
        )
        target_compile_definitions(${TARGET_NAME} PRIVATE _AFXDLL)
    endif()
    
    # MSVC settings - DLL runtime is set as default in root CMakeLists.txt
    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /W3)
        
        if(ARG_IS_DLL)
            target_link_options(${TARGET_NAME} PRIVATE /SUBSYSTEM:WINDOWS)
        else()
            target_link_options(${TARGET_NAME} PRIVATE /SUBSYSTEM:CONSOLE)
        endif()
    endif()
    
    # Precompiled headers
    if(ARG_HAS_PCH AND ARG_PCH_HEADER)
        target_precompile_headers(${TARGET_NAME} PRIVATE ${ARG_PCH_HEADER})
    endif()
    
    # Add to tests folder in IDE
    set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Tests")
    
    # Register with CTest if it's an executable (not a DLL)
    if(NOT ARG_IS_DLL)
        add_test(
            NAME ${TARGET_NAME}
            COMMAND ${TARGET_NAME}
            WORKING_DIRECTORY $<TARGET_FILE_DIR:${TARGET_NAME}>
        )
    endif()
endfunction()

# ============================================================================
# test_basics - Basic VLD tests
# ============================================================================
add_vld_test(test_basics
    SOURCES
        basics/Allocs.cpp
        basics/basics.cpp
        basics/basics_disabled.cpp
        basics/stdafx.cpp
    HEADERS
        basics/Allocs.h
        basics/stdafx.h
        basics/targetver.h
    DEPENDS gtest
    HAS_PCH
    PCH_HEADER basics/stdafx.h
)

# ============================================================================
# testsuite - Main test suite (ANSI build - CharacterSet=NotSet in original)
# ============================================================================
add_vld_test(testsuite
    NO_UNICODE
    SOURCES
        suite/testsuite.cpp
    DEPENDS gtest
)

# ============================================================================
# dynamic - Dynamic library for testing
# ============================================================================
add_vld_test(dynamic
    IS_DLL
    SOURCES
        dynamic_dll/dynamic.cpp
        dynamic_dll/stdafx.cpp
        basics/Allocs.cpp
    HEADERS
        dynamic_dll/dynamic.h
        dynamic_dll/stdafx.h
        dynamic_dll/targetver.h
        basics/Allocs.h
    DEPENDS gtest
    DEFINITIONS DYNAMIC_EXPORTS
    HAS_PCH
    PCH_HEADER dynamic_dll/stdafx.h
)

# ============================================================================
# dynamic_app - Application that uses dynamic DLL
# ============================================================================
add_vld_test(dynamic_app
    SOURCES
        dynamic_app/dynamic_app.cpp
        dynamic_app/LoadTests.cpp
        dynamic_app/ThreadTest.cpp
        dynamic_app/stdafx.cpp
    HEADERS
        dynamic_app/stdafx.h
        dynamic_app/targetver.h
    DEPENDS dynamic gtest
    HAS_PCH
    PCH_HEADER dynamic_app/stdafx.h
)

# ============================================================================
# corruption - Memory corruption tests
# ============================================================================
add_vld_test(corruption
    SOURCES
        corruption/corruption.cpp
        corruption/Tests.cpp
        corruption/stdafx.cpp
    HEADERS
        corruption/stdafx.h
        corruption/targetver.h
    DEPENDS gtest
    HAS_PCH
    PCH_HEADER corruption/stdafx.h
)

# ============================================================================
# vld_main - Main VLD test application
# ============================================================================
add_vld_test(vld_main
    SOURCES
        vld_main/vld_main.cpp
        vld_main/stdafx.cpp
    HEADERS
        vld_main/stdafx.h
        vld_main/targetver.h
    HAS_PCH
    PCH_HEADER vld_main/stdafx.h
)

# ============================================================================
# vld_main_test - Main VLD test with gtest
# ============================================================================
add_vld_test(vld_main_test
    SOURCES
        vld_main_test/vld_main_test.cpp
        vld_main_test/stdafx.cpp
    HEADERS
        vld_main_test/stdafx.h
        vld_main_test/targetver.h
    DEPENDS gtest
    HAS_PCH
    PCH_HEADER vld_main_test/stdafx.h
)
# vld_main_test spawns vld_main.exe - ensure it's built first
add_dependencies(vld_main_test vld_main)

# ============================================================================
# MFC Tests (only when VLD_USE_MFC is ON)
# ============================================================================
if(VLD_USE_MFC)
    # test_mfc - MFC DLL test
    add_vld_test(test_mfc
        IS_DLL
        USE_MFC
        SOURCES
            mfc_dll/mfc.cpp
            mfc_dll/stdafx.cpp
        HEADERS
            mfc_dll/mfc.h
            mfc_dll/stdafx.h
            mfc_dll/targetver.h
            mfc_dll/resource.h
        HAS_PCH
        PCH_HEADER mfc_dll/stdafx.h
    )
    
    set_target_properties(test_mfc PROPERTIES
        VS_MFC_FLAG 2
    )
    
    # Module definition file for MFC DLL
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/mfc_dll/mfc.def)
        target_sources(test_mfc PRIVATE mfc_dll/mfc.def)
    endif()
    
    # vldmfc - MFC Application test
    add_executable(vldmfc WIN32
        mfc/vldmfc.cpp
        mfc/vldmfcdlg.cpp
        mfc/StdAfx.cpp
        mfc/vldmfc.rc
        mfc/vldmfc.h
        mfc/vldmfcdlg.h
        mfc/resource.h
        mfc/StdAfx.h
    )
    
    target_include_directories(vldmfc PRIVATE ${VLD_TEST_INCLUDE_DIRS})
    target_link_libraries(vldmfc PRIVATE vld)
    
    target_compile_definitions(vldmfc PRIVATE
        VLD_FORCE_ENABLE
        _WINDOWS
        _AFXDLL
        UNICODE
        _UNICODE
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
    )
    
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_compile_definitions(vldmfc PRIVATE WIN64)
    else()
        target_compile_definitions(vldmfc PRIVATE WIN32)
    endif()
    
    # MFC settings - UseOfMfc=Dynamic corresponds to CMAKE_MFC_FLAG=2
    set_target_properties(vldmfc PROPERTIES
        FOLDER "Tests"
        VS_GLOBAL_KEYWORD "MFCProj"
    )
    # CMake's MFC support requires setting this variable before add_executable
    # But since we already added it, we need a workaround using VS generator expression
    set_property(TARGET vldmfc PROPERTY VS_GLOBAL_UseOfMfc "Dynamic")
    
    # MFC requires DLL runtime
    set_property(TARGET vldmfc PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    
    target_compile_options(vldmfc PRIVATE /W3)
    
    # MFC uses its own entry point - just set subsystem
    target_link_options(vldmfc PRIVATE /SUBSYSTEM:WINDOWS)
    
    target_precompile_headers(vldmfc PRIVATE mfc/StdAfx.h)
endif()

# ============================================================================
# Console test (optional, simpler test)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/console/main.cpp)
    add_vld_test(vldconsole
        SOURCES
            console/main.cpp
    )
endif()

# ============================================================================
# Static string test
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/static_string_test/static_string_test.cpp)
    file(GLOB STATIC_STRING_TEST_SOURCES static_string_test/*.cpp)
    file(GLOB STATIC_STRING_TEST_HEADERS static_string_test/*.h)
    add_vld_test(static_string_test
        SOURCES ${STATIC_STRING_TEST_SOURCES}
        HEADERS ${STATIC_STRING_TEST_HEADERS}
        DEPENDS gtest
    )
endif()

# ============================================================================
# Ignore functions test
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ignore_functions_test/ignore_functions_test.cpp)
    file(GLOB IGNORE_FUNCTIONS_TEST_SOURCES ignore_functions_test/*.cpp)
    file(GLOB IGNORE_FUNCTIONS_TEST_HEADERS ignore_functions_test/*.h)
    add_vld_test(ignore_functions_test
        SOURCES ${IGNORE_FUNCTIONS_TEST_SOURCES}
        HEADERS ${IGNORE_FUNCTIONS_TEST_HEADERS}
        DEPENDS gtest
    )
endif()

# ============================================================================
# VLD DLLs for unload testing
# ============================================================================
# vld_dll1
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vld_dll1)
    file(GLOB VLD_DLL1_SOURCES vld_dll1/*.cpp)
    file(GLOB VLD_DLL1_HEADERS vld_dll1/*.h)
    if(VLD_DLL1_SOURCES)
        add_vld_test(vld_dll1
            IS_DLL
            SOURCES ${VLD_DLL1_SOURCES}
            HEADERS ${VLD_DLL1_HEADERS}
        )
    endif()
endif()

# vld_dll2
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vld_dll2)
    file(GLOB VLD_DLL2_SOURCES vld_dll2/*.cpp)
    file(GLOB VLD_DLL2_HEADERS vld_dll2/*.h)
    if(VLD_DLL2_SOURCES)
        add_vld_test(vld_dll2
            IS_DLL
            SOURCES ${VLD_DLL2_SOURCES}
            HEADERS ${VLD_DLL2_HEADERS}
        )
    endif()
endif()

# vld_unload - Uses LoadLibrary at runtime, no link-time dependency on dll1/dll2
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vld_unload)
    file(GLOB VLD_UNLOAD_SOURCES vld_unload/*.cpp)
    file(GLOB VLD_UNLOAD_HEADERS vld_unload/*.h)
    if(VLD_UNLOAD_SOURCES)
        add_vld_test(vld_unload
            SOURCES ${VLD_UNLOAD_SOURCES}
            HEADERS ${VLD_UNLOAD_HEADERS}
            DEPENDS gtest
        )
        # Ensure dll1 and dll2 are built before vld_unload (runtime dependency)
        if(TARGET vld_dll1)
            add_dependencies(vld_unload vld_dll1)
        endif()
        if(TARGET vld_dll2)
            add_dependencies(vld_unload vld_dll2)
        endif()
    endif()
endif()

# ============================================================================
# COM Test - MFC/ATL COM DLL with IDL compilation
# ============================================================================
if(VLD_USE_MFC AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vld_ComTest/ComTest.idl)
    # Find MIDL compiler
    find_program(MIDL_EXECUTABLE midl)
    
    if(MIDL_EXECUTABLE)
        set(COMTEST_IDL ${CMAKE_CURRENT_SOURCE_DIR}/vld_ComTest/ComTest.idl)
        set(COMTEST_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/vld_ComTest)
        
        # Generated files from MIDL
        set(COMTEST_GENERATED_H ${COMTEST_GENERATED_DIR}/ComTest_i.h)
        set(COMTEST_GENERATED_C ${COMTEST_GENERATED_DIR}/ComTest_i.c)
        set(COMTEST_GENERATED_P ${COMTEST_GENERATED_DIR}/ComTest_p.c)
        set(COMTEST_GENERATED_TLB ${COMTEST_GENERATED_DIR}/ComTest.tlb)
        
        # Create output directory
        file(MAKE_DIRECTORY ${COMTEST_GENERATED_DIR})
        
        # Determine target environment for MIDL
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(MIDL_ENV "x64")
        else()
            set(MIDL_ENV "win32")
        endif()
        
        # Custom command to run MIDL compiler
        add_custom_command(
            OUTPUT ${COMTEST_GENERATED_H} ${COMTEST_GENERATED_C} ${COMTEST_GENERATED_P} ${COMTEST_GENERATED_TLB}
            COMMAND ${MIDL_EXECUTABLE}
                /nologo
                /env ${MIDL_ENV}
                /h ComTest_i.h
                /iid ComTest_i.c
                /proxy ComTest_p.c
                /tlb ComTest.tlb
                /Oicf
                /out ${COMTEST_GENERATED_DIR}
                ${COMTEST_IDL}
            DEPENDS ${COMTEST_IDL}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vld_ComTest
            COMMENT "Compiling IDL file: ComTest.idl"
            VERBATIM
        )
        
        # Custom target for IDL compilation
        add_custom_target(ComTest_idl DEPENDS 
            ${COMTEST_GENERATED_H} 
            ${COMTEST_GENERATED_C}
        )
        
        # C++ source files (use PCH)
        set(COMTEST_CPP_SOURCES
            vld_ComTest/ComTest.cpp
            vld_ComTest/dllmain.cpp
            vld_ComTest/MyMath.cpp
            vld_ComTest/stdafx.cpp
        )
        
        # C source files (no PCH - they're RPC/proxy stubs)
        # Note: xdlldata.c includes dlldata.c conditionally via _MERGE_PROXYSTUB
        # We only need xdlldata.c and the generated ComTest_i.c
        set(COMTEST_C_SOURCES
            vld_ComTest/xdlldata.c
            ${COMTEST_GENERATED_C}
        )
        
        # COM Test DLL
        add_library(ComTest SHARED
            ${COMTEST_CPP_SOURCES}
            ${COMTEST_C_SOURCES}
            vld_ComTest/ComTest.rc
            vld_ComTest/ComTest.def
        )
        
        # Exclude C files from precompiled header
        set_source_files_properties(${COMTEST_C_SOURCES} PROPERTIES
            SKIP_PRECOMPILE_HEADERS ON
        )
        
        # Ensure IDL is compiled before ComTest
        add_dependencies(ComTest ComTest_idl)
        
        target_include_directories(ComTest PRIVATE 
            ${VLD_TEST_INCLUDE_DIRS}
            ${COMTEST_GENERATED_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/vld_ComTest
        )
        
        target_link_libraries(ComTest PRIVATE vld)
        
        # Basic definitions - ATL macros are already in stdafx.h
        target_compile_definitions(ComTest PRIVATE
            _WINDOWS
            _USRDLL
            _AFXDLL
            UNICODE
            _UNICODE
            $<$<CONFIG:Debug>:_DEBUG>
            $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
        )
        
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            target_compile_definitions(ComTest PRIVATE WIN64)
        else()
            target_compile_definitions(ComTest PRIVATE WIN32)
        endif()
        
        # MFC settings
        set_target_properties(ComTest PROPERTIES
            FOLDER "Tests"
            VS_GLOBAL_KEYWORD "MFCProj"
        )
        set_property(TARGET ComTest PROPERTY VS_GLOBAL_UseOfMfc "Dynamic")
        
        # MFC requires DLL runtime
        set_property(TARGET ComTest PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        
        target_compile_options(ComTest PRIVATE /W3)
        target_link_options(ComTest PRIVATE /SUBSYSTEM:WINDOWS)
        
        # Precompiled header for C++ files only
        target_precompile_headers(ComTest PRIVATE 
            "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/vld_ComTest/stdafx.h>"
        )
        
        message(STATUS "COM Test enabled - MIDL found at: ${MIDL_EXECUTABLE}")
    else()
        message(WARNING "COM Test skipped - MIDL compiler not found")
    endif()
endif()
