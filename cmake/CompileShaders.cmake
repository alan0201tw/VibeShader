# CompileShaders.cmake — compile GLSL shaders to SPIR-V using glslc
#
# Usage:
#   compile_shaders(
#     TARGET <target_name>
#     SOURCES <shader1.vert> <shader2.frag> ...
#     OUTPUT_DIR <path>
#   )

function(compile_shaders)
  cmake_parse_arguments(SHADER "" "TARGET;OUTPUT_DIR" "SOURCES" ${ARGN})

  if(NOT SHADER_TARGET)
    message(FATAL_ERROR "compile_shaders: TARGET is required")
  endif()
  if(NOT SHADER_OUTPUT_DIR)
    message(FATAL_ERROR "compile_shaders: OUTPUT_DIR is required")
  endif()

  find_program(GLSLC_EXECUTABLE glslc HINTS Vulkan::glslc)
  if(NOT GLSLC_EXECUTABLE)
    # Fall back to Vulkan SDK path
    find_program(GLSLC_EXECUTABLE glslc
      HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
    )
  endif()
  if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found. Install the Vulkan SDK.")
  endif()

  file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

  set(SPIRV_OUTPUTS "")

  foreach(SHADER_SOURCE ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME "${SHADER_SOURCE}" NAME)
    set(SPIRV_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")

    add_custom_command(
      OUTPUT "${SPIRV_OUTPUT}"
      COMMAND "${GLSLC_EXECUTABLE}"
              -std=460
              --target-env=vulkan1.3
              -Werror
              -o "${SPIRV_OUTPUT}"
              "${SHADER_SOURCE}"
      DEPENDS "${SHADER_SOURCE}"
      COMMENT "Compiling GLSL → SPIR-V: ${SHADER_NAME}"
      VERBATIM
    )

    list(APPEND SPIRV_OUTPUTS "${SPIRV_OUTPUT}")
  endforeach()

  add_custom_target(${SHADER_TARGET}
    DEPENDS ${SPIRV_OUTPUTS}
  )
endfunction()
