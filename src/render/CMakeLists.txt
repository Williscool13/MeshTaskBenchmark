
add_library(render-lib STATIC
        vk_context.cpp
        vk_context.h
)

target_link_libraries(render-lib PUBLIC
        engine-common
        volk
        VulkanMemoryAllocator
        vk-bootstrap::vk-bootstrap
        SDL3::SDL3
        fastgltf
        offsetAllocator
        meshoptimizer
)

find_program(SLANGC slangc REQUIRED)


function(compile_shader TARGET SHADER_SOURCE STAGE ENTRY)
    get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME_WE)
    set(SHADER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}_${STAGE}.spv)
    set(COMMON_SHADER_INCLUDES ${CMAKE_SOURCE_DIR}/shaders/common.slang)
    add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
            COMMAND ${SLANGC} ${SHADER_SOURCE} -target spirv -entry ${ENTRY} -stage ${STAGE} -o ${SHADER_OUTPUT}
            DEPENDS ${SHADER_SOURCE} ${COMMON_SHADER_INCLUDES}
            COMMENT "Compiling ${STAGE} shader from ${SHADER_SOURCE}"
    )

    target_sources(${TARGET} PRIVATE ${SHADER_OUTPUT})
endfunction()


function(compile_shaders TARGET)
    set(ARGS ${ARGN})
    list(LENGTH ARGS NUM_ARGS)

    math(EXPR NUM_TRIPLETS "${NUM_ARGS} / 3")

    foreach (i RANGE 0 ${NUM_TRIPLETS})
        math(EXPR SOURCE_IDX "${i} * 3")
        math(EXPR STAGE_IDX "${i} * 3 + 1")
        math(EXPR ENTRY_IDX "${i} * 3 + 2")

        if (SOURCE_IDX LESS NUM_ARGS)
            list(GET ARGS ${SOURCE_IDX} SHADER_SOURCE)
            list(GET ARGS ${STAGE_IDX} STAGE)
            list(GET ARGS ${ENTRY_IDX} ENTRY)
            compile_shader(${TARGET} ${SHADER_SOURCE} ${STAGE} ${ENTRY})
        endif ()
    endforeach ()
endfunction()