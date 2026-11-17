project(switch_desktop_1 LANGUAGES CXX RC)

add_executable(switch_desktop_1 WIN32
    main.cpp
    version.rc
)

target_link_libraries(switch_desktop_1 PRIVATE
    desktop_tools_common
)

# Output binaries directly into the project's bin directory
set_target_properties(switch_desktop_1 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
)
