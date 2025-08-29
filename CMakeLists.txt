cmake_minimum_required(VERSION 3.31)
project(splinegen)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

set(SFML_VERSION 3.0.0)
set(IMGUI_VERSION 1.91.1)
set(IMGUI_SFML_VERSION 3.0)

FetchContent_Declare(
        SFML
        URL "https://github.com/SFML/SFML/archive/${SFML_VERSION}.zip"
)

FetchContent_Declare(
        imgui
        URL "https://github.com/ocornut/imgui/archive/v${IMGUI_VERSION}.zip"
)

FetchContent_Declare(
        imgui-sfml
        URL "https://github.com/SFML/imgui-sfml/archive/v${IMGUI_SFML_VERSION}.zip"
)

option(SFML_BUILD_AUDIO "Build audio" OFF)
option(SFML_BUILD_NETWORK "Build network" OFF)
FetchContent_MakeAvailable(sfml)

FetchContent_MakeAvailable(imgui)

set(IMGUI_DIR ${imgui_SOURCE_DIR})
option(IMGUI_SFML_FIND_SFML "Use find_package to find SFML" OFF)
option(IMGUI_SFML_IMGUI_DEMO "Build imgui_demo.cpp" ON)
FetchContent_MakeAvailable(imgui-sfml)

include_directories(${CMAKE_SOURCE_DIR}/libs)

add_executable(splinegen src/main.cpp src/app.cpp src/drawer.cpp
        src/TextContainer.h)
target_link_libraries(splinegen PRIVATE ImGui-SFML::ImGui-SFML)
