cmake_minimum_required(VERSION 3.10)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

# If Visual Studio 2015 (MSVC 19.0) is used, ensure Windows SDK 8.1 is targeted
if(MSVC AND MSVC_VERSION EQUAL 1900)
    if(NOT DEFINED CMAKE_SYSTEM_VERSION)
        set(CMAKE_SYSTEM_VERSION "8.1" CACHE STRING "Windows SDK Version" FORCE)
    endif()
endif()

project(desktop_tools VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure static C/C++ runtime library (/MT for Release, /MTd for Debug)
# This creates fully self-contained standalone executables with zero external CRT DLL dependencies.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    foreach(flag_var
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        if(${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif()
    endforeach()
endif()

# Global output directory for executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(CONFIG_NAME ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG_NAME} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_BINARY_DIR}/bin")
endforeach()

# Subprojects
add_subdirectory(common)
add_subdirectory(switch_desktop_1)
add_subdirectory(switch_desktop_2)
add_subdirectory(focus_chatgpt)
