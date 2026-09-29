cmake_minimum_required(VERSION 3.16)
project(Bezier LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_VERSION VERSION_LESS "3.28")
    include(${CMAKE_SOURCE_DIR}/cmake/ImportSFML.cmake)
else ()
    include(FetchContent)
    FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 3.1.0
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL
        SYSTEM
    )
    set(SFML_USE_SYSTEM_DEPS OFF)
    FetchContent_MakeAvailable(SFML)
endif ()

add_executable(Bezier src/main.cpp)
target_link_libraries(Bezier PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::Audio
)
