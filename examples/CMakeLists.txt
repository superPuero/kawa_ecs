add_executable(kawa_ecs_examples examples.cpp)
target_link_libraries(kawa_ecs_examples PRIVATE kawa_ecs)

add_executable(kawa_ecs_stress_test stress_test.cpp)
target_link_libraries(kawa_ecs_stress_test PRIVATE kawa_ecs)

target_compile_features(kawa_ecs_examples PRIVATE cxx_std_20)
target_compile_features(kawa_ecs_stress_test PRIVATE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(kawa_ecs_examples PRIVATE -Wall -Wextra)
    target_compile_options(kawa_ecs_stress_test PRIVATE -Wall -Wextra)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(kawa_ecs_examples PRIVATE /W4)
    target_compile_options(kawa_ecs_stress_test PRIVATE /W4)
endif()