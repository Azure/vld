# Set the include directory
set(VLD_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../include")

# Set the library directory
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    # 64-bit
    set(VLD_LIBRARY "${CMAKE_CURRENT_LIST_DIR}/../lib/Win64/vld.lib")
else()
    # 32-bit
    set(VLD_LIBRARY "${CMAKE_CURRENT_LIST_DIR}/../lib/Win32/vld.lib")
endif()

# Provide the include directories and libraries
set(VLD_INCLUDE_DIRS ${VLD_INCLUDE_DIR})
set(VLD_LIBRARIES ${VLD_LIBRARY})

# Mark variables as advanced
mark_as_advanced(VLD_INCLUDE_DIR VLD_LIBRARY)