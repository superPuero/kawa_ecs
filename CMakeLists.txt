cmake_minimum_required(VERSION 3.28)

project(
    kawa_ecs
    DESCRIPTION "header-only ECS"
    LANGUAGES CXX
)

option(KAWA_ECS_BUILD_MODULES "Build C++ modules" OFF)
option(KAWA_ECS_BUILD_EXAMPLES "Build examples" OFF)

add_library(kawa_ecs INTERFACE)

set_property(
    TARGET kawa_ecs PROPERTY
    EXPORT_NAME kawa_ecs
)

target_include_directories(
    kawa_ecs
    INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_compile_features(
    kawa_ecs
    INTERFACE
    cxx_std_20
)

if(KAWA_ECS_BUILD_MODULES)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.28)
        message(STATUS "Building C++ modules")
        add_subdirectory(src)
    else()
        message(WARNING "Skipping C++ modules (requires CMake 3.28+, found ${CMAKE_VERSION})")
    endif()
else()
    message(STATUS "C++ modules support is disabled. Enable with -DKAWA_ECS_BUILD_MODULES=ON")
endif()

if(KAWA_ECS_BUILD_EXAMPLES)
    message(STATUS "Building examples")
    add_subdirectory(examples)
endif()
