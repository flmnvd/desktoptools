project(focus_chatgpt LANGUAGES CXX RC)

add_executable(focus_chatgpt WIN32
    main.cpp
    chatgpt_manager.h
    chatgpt_manager.cpp
    version.rc
)

target_link_libraries(focus_chatgpt PRIVATE
    desktop_tools_common
    user32
    ole32
    shell32
    advapi32
    dwmapi
)

# Output binaries directly into the project's bin directory
set_target_properties(focus_chatgpt PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
)
