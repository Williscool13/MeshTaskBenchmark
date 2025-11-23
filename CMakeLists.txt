cmake_minimum_required(VERSION 4.0)
project(MeshTaskBenchmark)

set(CMAKE_CXX_STANDARD 20)

add_compile_definitions(NOMINMAX)
add_definitions(-DGLM_FORCE_DEPTH_ZERO_TO_ONE)
add_definitions(-DVK_NO_PROTOTYPES)

############## Dependencies ##############
add_subdirectory(extern/fmt)
add_subdirectory(extern/SDL EXCLUDE_FROM_ALL)
set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
add_subdirectory(extern/volk)
add_subdirectory(extern/vma)
add_subdirectory(extern/vk-bootstrap)
add_subdirectory(extern/fastgltf)
add_subdirectory(extern/OffsetAllocator)
add_subdirectory(extern/meshoptimizer)
###########################################

############## Internal Libs ##############
add_subdirectory(src/core)   # core-lib
add_subdirectory(src/game)   # game-lib
add_subdirectory(src/render) # render-lib
add_subdirectory(src/input)  # input-lib
###########################################

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
        core-lib
        game-lib
        render-lib
        input-lib
)


add_custom_command(TARGET meshTaskBenchmark POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:meshTaskBenchmark>
)

compile_shaders(meshTaskBenchmark
        ${CMAKE_SOURCE_DIR}/shaders/traditional.slang vertex vertexMain
        ${CMAKE_SOURCE_DIR}/shaders/traditional.slang fragment fragmentMain
        ${CMAKE_SOURCE_DIR}/shaders/indirectTraditional.slang compute computeMain
        ${CMAKE_SOURCE_DIR}/shaders/indirectTraditional.slang vertex vertexMain
        ${CMAKE_SOURCE_DIR}/shaders/indirectTraditional.slang fragment fragmentMain
        ${CMAKE_SOURCE_DIR}/shaders/basicMeshShader.slang task taskMain
        ${CMAKE_SOURCE_DIR}/shaders/basicMeshShader.slang mesh meshMain
        ${CMAKE_SOURCE_DIR}/shaders/basicMeshShader.slang fragment fragmentMain
)


############## Engine Common ##############
add_library(engine-common STATIC
        src/utils.cpp
        src/utils.h)

target_compile_definitions(engine-common PUBLIC
        BUILD_CONFIG_NAME="$<CONFIG>"
)

target_include_directories(engine-common PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/extern
)

target_link_libraries(engine-common PUBLIC
        fmt::fmt
)
###########################################