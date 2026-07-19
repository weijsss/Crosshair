cmake_minimum_required(VERSION 3.14)
project(Crosshair VERSION 3.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/utf-8 /O2 /GS- /MT)
    add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
    add_link_options(/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
else()
    add_compile_options(-O2 -s)
    add_link_options(-mwindows -static -static-libgcc -static-libstdc++)
endif()

add_executable(Crosshair
    main.cpp
    config.cpp
    render.cpp
    overlay.cpp
    settings.cpp
    resource.rc
)

target_link_libraries(Crosshair gdiplus user32 gdi32 comctl32 comdlg32 ole32 shell32)
