# EmbedShader.cmake - Convert SPIR-V binary to C++ header
file(READ ${SHADER_SPV} SHADER_BINARY HEX)

# Convert hex string to comma-separated byte array
string(LENGTH "${SHADER_BINARY}" HEX_LENGTH)
math(EXPR BYTE_COUNT "${HEX_LENGTH} / 2")

set(BYTE_ARRAY "")
set(LINE_BYTES 0)

math(EXPR LAST_INDEX "${HEX_LENGTH} - 2")
foreach(IDX RANGE 0 ${LAST_INDEX} 2)
    string(SUBSTRING "${SHADER_BINARY}" ${IDX} 2 HEX_BYTE)
    if(BYTE_ARRAY)
        string(APPEND BYTE_ARRAY ",")
        if(LINE_BYTES GREATER_EQUAL 16)
            string(APPEND BYTE_ARRAY "\n    ")
            set(LINE_BYTES 0)
        endif()
    endif()
    string(APPEND BYTE_ARRAY "0x${HEX_BYTE}")
    math(EXPR LINE_BYTES "${LINE_BYTES} + 1")
endforeach()

# Calculate uint32 count for SPIR-V (must be multiple of 4 bytes)
math(EXPR UINT32_COUNT "${BYTE_COUNT} / 4")

file(WRITE ${SHADER_HEADER}
"// Auto-generated shader header - do not edit
#pragma once
#include <cstdint>
#include <cstddef>

alignas(4) static const uint8_t ${SHADER_VAR_NAME}_data[] = {
    ${BYTE_ARRAY}
};

static const size_t ${SHADER_VAR_NAME}_size = ${BYTE_COUNT};
")
