# ===============================================
# COMPONENT GENERATION (.pgcomp -> C++)
# ===============================================
#
# columba_generate_components(
#     DEFINITIONS <file.pgcomp>...
#     OUTPUT_DIR <dir>
#     [SOURCES_VAR <var>]
#     [OUTPUTS_VAR <var>]
#     [TARGET_PREFIX <prefix>]
# )
#
# Runs tools/component_generator.pg with PgCompilerBootstrap on every definition
# and writes <Name>.generated.h, <Name>.generated.cpp and <Name>.serialization.cpp
# into OUTPUT_DIR. A component regenerates only when its definition or the
# generator changes.
#
#   SOURCES_VAR    receives the generated .cpp files, to add to the consuming target
#   OUTPUTS_VAR    receives every generated file, headers included
#   TARGET_PREFIX  also creates one <prefix><Name> target per component
#
# The caller adds OUTPUT_DIR to the include directories of the consuming target.
# Native builds only: the bootstrap compiler cannot run on the host under Emscripten.

get_filename_component(COLUMBA_TOOLS_DIR "${CMAKE_CURRENT_LIST_DIR}/../tools" ABSOLUTE)

function(columba_generate_components)
    cmake_parse_arguments(ARG "" "OUTPUT_DIR;SOURCES_VAR;OUTPUTS_VAR;TARGET_PREFIX" "DEFINITIONS" ${ARGN})

    if(NOT ARG_OUTPUT_DIR)
        message(FATAL_ERROR "columba_generate_components: OUTPUT_DIR is required")
    endif()

    if(NOT TARGET PgCompilerBootstrap)
        message(FATAL_ERROR "columba_generate_components: the PgCompilerBootstrap target does not exist (native builds only)")
    endif()

    file(MAKE_DIRECTORY ${ARG_OUTPUT_DIR})

    set(GENERATED_SOURCES "")
    set(GENERATED_OUTPUTS "")

    foreach(COMP_DEF ${ARG_DEFINITIONS})
        # The generator runs from OUTPUT_DIR, so the definition has to be absolute
        get_filename_component(COMP_DEF "${COMP_DEF}" ABSOLUTE)
        get_filename_component(COMP_NAME ${COMP_DEF} NAME_WE)

        set(GENERATED_HEADER "${ARG_OUTPUT_DIR}/${COMP_NAME}.generated.h")
        set(GENERATED_CPP "${ARG_OUTPUT_DIR}/${COMP_NAME}.generated.cpp")
        set(GENERATED_SER "${ARG_OUTPUT_DIR}/${COMP_NAME}.serialization.cpp")

        # Only runs when the inputs change
        add_custom_command(
            OUTPUT ${GENERATED_HEADER} ${GENERATED_CPP} ${GENERATED_SER}
            COMMAND ${CMAKE_COMMAND} -E echo "Regenerating ${COMP_NAME}..."
            COMMAND $<TARGET_FILE:PgCompilerBootstrap>
                    ${COLUMBA_TOOLS_DIR}/component_generator.pg
                    ${COMP_DEF}
            DEPENDS
                PgCompilerBootstrap
                ${COMP_DEF}
                ${COLUMBA_TOOLS_DIR}/component_generator.pg
                ${COLUMBA_TOOLS_DIR}/yaml_parser.pg
            WORKING_DIRECTORY ${ARG_OUTPUT_DIR}
            COMMENT "Regenerating ${COMP_NAME} from ${COMP_DEF}"
            VERBATIM
        )

        # Mark the generated files as GENERATED so CMake knows they'll be created
        set_source_files_properties(
            ${GENERATED_HEADER} ${GENERATED_CPP} ${GENERATED_SER}
            PROPERTIES GENERATED TRUE
        )

        # Per component target, for manual regeneration
        if(ARG_TARGET_PREFIX)
            add_custom_target(${ARG_TARGET_PREFIX}${COMP_NAME}
                DEPENDS ${GENERATED_HEADER} ${GENERATED_CPP} ${GENERATED_SER}
            )
        endif()

        list(APPEND GENERATED_SOURCES ${GENERATED_CPP} ${GENERATED_SER})
        list(APPEND GENERATED_OUTPUTS ${GENERATED_HEADER} ${GENERATED_CPP} ${GENERATED_SER})
    endforeach()

    if(ARG_SOURCES_VAR)
        set(${ARG_SOURCES_VAR} "${GENERATED_SOURCES}" PARENT_SCOPE)
    endif()

    if(ARG_OUTPUTS_VAR)
        set(${ARG_OUTPUTS_VAR} "${GENERATED_OUTPUTS}" PARENT_SCOPE)
    endif()
endfunction()
