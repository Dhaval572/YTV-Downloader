cmake_minimum_required(VERSION 3.27)
project(Youtube_Video_Downloader)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Try to find a locally installed raylib
find_package(raylib 5.5 QUIET)

include(FetchContent)
if (NOT raylib_FOUND)
    FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP True
    )
    FetchContent_MakeAvailable(raylib)
endif()

# Collect source files only (.cpp)
file(GLOB PROJECT_SRC_CPP "src/*.cpp")

# rlImGui and ImGui sources
set(RLIMGUI_SRC rlImGui/rlImGui.cpp)
set(IMGUI_SRC imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/imgui_demo.cpp)
set(TINYFILEDIALOGS tinyfiledialogs/tinyfiledialogs.c)

# Copy assets to the build directory
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
endif()

# Create executable
add_executable(main ${PROJECT_SRC_CPP} ${RLIMGUI_SRC} ${IMGUI_SRC} ${TINYFILEDIALOGS})

# Add include directories — this is what lets you include .hpp/.h files
target_include_directories(main PRIVATE
    ${raylib_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/imgui
    ${CMAKE_SOURCE_DIR}/rlImGui
    ${CMAKE_SOURCE_DIR}/tinyfiledialogs
    ${CMAKE_SOURCE_DIR}/src  # <-- This is what includes your .h/.hpp headers!
)

# Link libraries
target_link_libraries(main raylib)
