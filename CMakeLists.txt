cmake_minimum_required(VERSION 4.1)
project(WindowsInfo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(WindowsInfoLib STATIC window_save_manager.cpp)
target_include_directories(WindowsInfoLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WindowsInfoLib PUBLIC user32)

add_executable(WindowsInfo)
target_link_libraries(WindowsInfo PRIVATE WindowsInfoLib)
