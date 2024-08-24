set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
set(SPIRV_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/assets/SPIRV")
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "*.frag.glsl"
    "*.vert.glsl"
    "*.comp.glsl"
    "*.geom.glsl"
    )

file(GLOB_RECURSE GLSL_INCLUDE_FILES
    "*.glsl"
    "*.glsl"
    "*.glsl"
    "*.glsl"
    )
 
foreach(GLSL ${GLSL_SOURCE_FILES})
    # get only the first part of filename
    # strip extension and directory
    cmake_path(GET GLSL STEM LAST_ONLY FILE_NAME)
    set(SPIRV "${SPIRV_OUTPUT_DIR}/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_OUTPUT_DIR}"
        COMMAND ${GLSL_VALIDATOR} -V --target-env vulkan1.3 ${GLSL} -o ${SPIRV} -gVS
        DEPENDS ${GLSL}
        DEPENDS ${GLSL_INCLUDE_FILES}
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)
add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(${PROJECT_NAME} Shaders)
