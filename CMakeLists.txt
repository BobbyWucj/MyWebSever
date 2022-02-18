cmake_minimum_required(VERSION 3.0.0)
project(TinyWebServer)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(CXX_FLAGS
        -Wall
        -Wextra
        -Werror
        -std=c++11)
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

add_subdirectory(web_server)

include_directories(./web_server)

add_executable(TinyWebServer main.cpp)
target_link_libraries(TinyWebServer webserver)