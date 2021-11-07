# MODE 1: Run as a custom target's script. -------------------------------------
if(BUILD_SHADER_TARGET)
    # Determine platform-specific shader compiler variables.
    if(APPLE)
        set(SHADERC_PLATFORM "osx")
        set(SHADERC_TARGETS  "glsl,430" "mtl,metal")
    elseif(UNIX)
        set(SHADERC_PLATFORM "linux")
        set(SHADERC_TARGETS  "glsl,430")
    elseif(WIN32)
        set(SHADERC_PLATFORM "windows")
        set(SHADERC_TARGETS  "glsl,430" "dx11,s_5_0")
    else()
        message(FATAL_ERROR "Unknown platform. Must be Apple, Linux or Windows.")
    endif()

    # Verify the shared BGFX shader resources and the compiler binary are at the
    # expected locations.
    set(BGFX_DIR     "${CMAKE_CURRENT_LIST_DIR}/../../../third_party/bgfx")
    set(SRC_DIR      "${BGFX_DIR}/src")
    set(COMMON_DIR   "${BGFX_DIR}/examples/common")
    set(SHADERC_FILE "${CMAKE_CURRENT_LIST_DIR}/../../../tools/shaderc/${SHADERC_PLATFORM}/shaderc")

    if(WIN32)
        set(SHADERC_FILE "${SHADERC_FILE}.exe")
    endif()

    if(NOT EXISTS "${SRC_DIR}/bgfx_shader.sh")
        message(FATAL_ERROR "Could not locate file: bgfx_shader.sh")
    elseif(NOT EXISTS "${COMMON_DIR}/shaderlib.sh")
        message(FATAL_ERROR "Could not locate file: shaderlib.sh")
    elseif(NOT EXISTS "${SHADERC_FILE}")
        message(FATAL_ERROR "Could not locate file: shaderc(.exe)")
    endif()

    # Compiles the shader source file into a single platform.
    function(compile_shader RELDIR SHADER_SUBPATH VARYING PLATFORM PROFILE SUBDIR OUTPUT_DIR)
        # Input file.
        set(INPUT_FILE "${RELDIR}/${SHADER}")
        if (NOT EXISTS ${INPUT_FILE})
            message(FATAL_ERROR "Could not locate file: ${SHADER}")
        endif()

        # Output file.
        get_filename_component(OUTPUT_SUBDIR ${SHADER_SUBPATH} DIRECTORY)
        get_filename_component(OUTPUT_FILE ${SHADER_SUBPATH} NAME)
        string(REPLACE "." "_" OUTPUT_FILE ${OUTPUT_FILE})
        set(FULL_OUTPUT_SUBDIR "${OUTPUT_DIR}/${OUTPUT_SUBDIR}/${SUBDIR}")
        set(FULL_OUTPUT_FILE "${FULL_OUTPUT_SUBDIR}/${OUTPUT_FILE}_${SUBDIR}.h")

        # Ensure the output directory exists.
        file(MAKE_DIRECTORY "${FULL_OUTPUT_SUBDIR}")

        # Shader type.
        get_filename_component(TYPE ${SHADER_SUBPATH} LAST_EXT)
        string(TOLOWER "${TYPE}" TYPE)

        if(TYPE STREQUAL ".vs")
            set(TYPE "vertex")
        elseif(TYPE STREQUAL ".fs")
            set(TYPE "fragment")
        else()
            message(FATAL_ERROR "Invalid or unsupported shader type '${TYPE}'.")
        endif()

        # Varying file.
        if(NOT VARYING)
            get_filename_component(VARYING ${INPUT_FILE} DIRECTORY)
            set(VARYING "${VARYING}/varying.def.sc")
        else()
            set(VARYING "${RELDIR}/${VARYING}")
        endif()

        # Skip recompilation if the source file hasn't changed. Note that this
        # will also erroneously skip recompilation if an included file has
        # changed instead.
        if(EXISTS "${FULL_OUTPUT_FILE}" AND
            "${FULL_OUTPUT_FILE}" IS_NEWER_THAN "${INPUT_FILE}" AND
            (NOT VARYING OR "${FULL_OUTPUT_FILE}" IS_NEWER_THAN "${VARYING}")
        )
            return()
        endif()

        # Specify the shader profil (vertex / pixel) for DirectX shaders.
        if(SUBDIR MATCHES "^dx")
            if(TYPE STREQUAL "vertex")
                set(PROFILE "v${PROFILE}")
            elseif(TYPE STREQUAL "fragment")
                set(PROFILE "p${PROFILE}")
            else()
                message(FATAL_ERROR "Invalid DirectX shader profile.")
            endif()
        endif()

        # Setup the compilation command.
        set(CMD "")
        string(APPEND CMD
            " -f \"${INPUT_FILE}\""
            " -o \"${FULL_OUTPUT_FILE}\""
            " --type ${TYPE}"
            " --bin2c"
            " -i \"${SRC_DIR}\""
            " -i \"${COMMON_DIR}\""
            " --platform ${PLATFORM}"
            " --profile ${PROFILE}"
            " --varyingdef ${VARYING}"
        )

        # Execute it.
        set(INFO_MESSAGE "Compiling shader: ${SHADER_SUBPATH} (${SUBDIR})")
        message(STATUS "${INFO_MESSAGE}")

        exec_program("\"${SHADERC_FILE}\"" # "${CMAKE_CURRENT_LIST_DIR}"
            ARGS "${CMD}"
            OUTPUT_VARIABLE OUTPUT
            RETURN_VALUE RESULT
        )
        if(NOT ${RESULT} EQUAL 0)
            message(FATAL_ERROR "Compilation failed:\n${OUTPUT}")
        endif()

        # Create the embeddable shader header file.
        get_filename_component(SHADER_NAME ${OUTPUT_FILE} NAME_WLE)
        string(TOUPPER "${SHADER_NAME}" SHADER_NAME_ALL_CAPS)

        set(SHADER_HEADER "${FULL_OUTPUT_SUBDIR}/../${SHADER_NAME}.h")

        if(NOT EXISTS "${SHADER_HEADER}")
            configure_file("${SHADER_HEADER_CONFIG}" ${SHADER_HEADER})
        endif()
    endfunction()

    # Iterate over the supported platforms and compile into each of them.
    foreach(SHADERC_TARGET IN LISTS SHADERC_TARGETS)
        string(REPLACE "," ";" TARGET_INFO ${SHADERC_TARGET})
        list(GET TARGET_INFO 0 SHADERC_SUBDIR)
        list(GET TARGET_INFO 1 SHADERC_PROFILE)

        compile_shader(
            ${RELDIR}
            ${SHADER}
            "${VARYING}"
            ${SHADERC_PLATFORM}
            ${SHADERC_PROFILE}
            ${SHADERC_SUBDIR}
            ${OUTPUT_DIR}
        )
    endforeach()

# MODE 2: Run as a part of the build process. ----------------------------------
else()
    function(add_shader_target TARGET SHADER VARYING RELDIR SCRIPT SHADER_HEADER_CONFIG OUTPUT_DIR)
        string(REGEX REPLACE "[\\/\.]+" "_" NAME ${SHADER})
    
        add_custom_target("${NAME}"
            COMMAND ${CMAKE_COMMAND}
                -D "BUILD_SHADER_TARGET=1"
                -D "SHADER=${SHADER}"
                -D "VARYING=${VARYING}"
                -D "RELDIR=\"${RELDIR}\""
                -D "OUTPUT_DIR=\"${OUTPUT_DIR}\""
                -D "SHADER_HEADER_CONFIG=\"${SHADER_HEADER_CONFIG}\""
                -P "${SCRIPT}"
        )

        add_dependencies(${TARGET} "${NAME}")
    endfunction()

    set(SHADER_TARGET_SCRIPT "${CMAKE_CURRENT_LIST_FILE}")
    set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders") # TODO : Expose overriding this via some config variable.
    set(SHADER_HEADER_CONFIG "${CMAKE_CURRENT_LIST_DIR}/shader.h.in")

    macro(add_shader_dependency TARGET SHADER)
        add_shader_target(
            ${TARGET}
            ${SHADER}
            "${ARGV2}"
            "\"${CMAKE_CURRENT_LIST_DIR}\""
            "\"${SHADER_TARGET_SCRIPT}\""
            "\"${SHADER_HEADER_CONFIG}\""
            "\"${SHADER_OUTPUT_DIR}\""
        )

        target_include_directories(${TARGET} PRIVATE
            ${SHADER_OUTPUT_DIR}
        )
    endmacro()
endif()