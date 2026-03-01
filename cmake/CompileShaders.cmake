# Find glslc shader compiler
find_program(GLSLC_EXECUTABLE glslc
    HINTS
        $ENV{VULKAN_SDK}/bin
        ${Vulkan_INCLUDE_DIR}/../bin
)

if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found. Please install the Vulkan SDK.")
endif()

message(STATUS "Found glslc: ${GLSLC_EXECUTABLE}")

function(compile_shaders SHADER_SOURCE_LIST COMPILED_SHADER_LIST)
    set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

    set(COMPILED_SHADERS "")

    foreach(SHADER_SRC IN LISTS ${SHADER_SOURCE_LIST})
        get_filename_component(SHADER_NAME ${SHADER_SRC} NAME)
        string(REPLACE "." "_" SHADER_VAR_NAME ${SHADER_NAME})
        set(SHADER_SPV "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
        set(SHADER_HEADER "${SHADER_OUTPUT_DIR}/${SHADER_VAR_NAME}.h")

        add_custom_command(
            OUTPUT ${SHADER_SPV}
            COMMAND ${GLSLC_EXECUTABLE} -O ${SHADER_SRC} -o ${SHADER_SPV}
            DEPENDS ${SHADER_SRC}
            COMMENT "Compiling shader: ${SHADER_NAME}"
            VERBATIM
        )

        # Generate C++ header with embedded SPIR-V
        add_custom_command(
            OUTPUT ${SHADER_HEADER}
            COMMAND ${CMAKE_COMMAND}
                -DSHADER_SPV=${SHADER_SPV}
                -DSHADER_HEADER=${SHADER_HEADER}
                -DSHADER_VAR_NAME=${SHADER_VAR_NAME}
                -P ${CMAKE_SOURCE_DIR}/cmake/EmbedShader.cmake
            DEPENDS ${SHADER_SPV}
            COMMENT "Embedding shader: ${SHADER_NAME}"
            VERBATIM
        )

        list(APPEND COMPILED_SHADERS ${SHADER_HEADER})
    endforeach()

    add_custom_target(compile_shaders DEPENDS ${COMPILED_SHADERS})
    set(${COMPILED_SHADER_LIST} ${COMPILED_SHADERS} PARENT_SCOPE)
endfunction()
