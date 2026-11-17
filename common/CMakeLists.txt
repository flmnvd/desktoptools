add_library(desktop_tools_common STATIC
    virtual_desktop.h
    virtual_desktop.cpp
)

target_include_directories(desktop_tools_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(desktop_tools_common PUBLIC
    ole32
    user32
    shell32
)
