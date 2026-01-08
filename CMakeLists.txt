cmake_minimum_required(VERSION 3.14)

message(WARNING ">>>> I AM RUNNING! The library CMakeLists.txt is being executed!")

project(kawa_core_prj VERSION 1.0.0 LANGUAGES CXX)

add_library(kawa_core INTERFACE)

add_library(kawa::core ALIAS kawa_core)

target_include_directories(kawa_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(kawa_core INTERFACE cxx_std_20)