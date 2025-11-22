cmake_minimum_required(VERSION 4.0)
project(MeshTaskBenchmark)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(extern/fmt)
add_subdirectory(extern/SDL EXCLUDE_FROM_ALL)
set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
add_subdirectory(extern/volk)
add_subdirectory(extern/vma)
add_subdirectory(extern/vk-bootstrap)
add_subdirectory(extern/fastgltf)
add_subdirectory(extern/OffsetAllocator)
add_subdirectory(extern/meshoptimizer)

add_subdirectory(src/render)
add_subdirectory(src/input)

add_executable(meshTaskBenchmark
        main.cpp
        mesh_task_benchmark.cpp
        mesh_task_benchmark.h
)

target_include_directories(meshTaskBenchmark PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/extern
        ${CMAKE_SOURCE_DIR}/extern/volk
        ${CMAKE_SOURCE_DIR}/extern/SDL/include
        ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(meshTaskBenchmark PUBLIC
        engine-common
        render-lib
        input-lib
)


add_custom_command(TARGET meshTaskBenchmark POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:meshTaskBenchmark>
)


############## Engine Common ##############
add_library(engine-common STATIC
        src/utils.cpp
        src/utils.h)

target_compile_definitions(engine-common PUBLIC
        BUILD_CONFIG_NAME="$<CONFIG>"
)

target_include_directories(engine-common PUBLIC
        ${CMAKE_SOURCE_DIR}/extern
)

target_link_libraries(engine-common PUBLIC
        fmt::fmt
)
###########################################



#compile_shaders(template
#        ${CMAKE_SOURCE_DIR}/shaders/render2.slang vertex vertexMain
#        ${CMAKE_SOURCE_DIR}/shaders/render2.slang fragment fragmentMain
#)