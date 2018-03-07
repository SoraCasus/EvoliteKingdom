cmake_minimum_required(VERSION 3.9)
project(Handmade)

set(CMAKE_CXX_STANDARD 17)


add_executable(Handmade WIN32 Win32_EvoliteKingdom.cpp)

target_link_libraries(Handmade user32.lib gdi32.lib)
