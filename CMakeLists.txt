cmake_minimum_required(VERSION 3.10)
project(zygisk_module CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(zygisk SHARED zygisk.cpp)

set_target_properties(zygisk PROPERTIES PREFIX "lib")

# بدون البحث عن JNI
target_include_directories(zygisk PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    /usr/include
)

target_link_libraries(zygisk log dl)
