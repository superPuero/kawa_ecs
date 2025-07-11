file(GLOB_RECURSE KAWA_ECS_MODULES *.cppm)

add_library(kawa_ecs_modules)
target_sources(kawa_ecs_modules
    PUBLIC
    FILE_SET CXX_MODULES FILES
    ${KAWA_ECS_MODULES}
)

target_link_libraries(kawa_ecs_modules
    PUBLIC
    kawa_ecs
)

target_include_directories(kawa_ecs_modules
    PUBLIC
    ${kawa_ecs_SOURCE_DIR}/include
)

target_compile_features(kawa_ecs_modules PUBLIC cxx_std_20)

if(KAWA_ECS_ENABLE_INSTALL)
    install(TARGETS kawa_ecs_modules
        EXPORT kawa_ecs-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/kawa_ecs/modules
    )
endif()