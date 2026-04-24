# Pure vendoring approach - everything is self-contained
cmake_minimum_required(VERSION 3.18)
project(ColumbaEngine VERSION 1.0)

# Options remain the same
option(USE_GDB "Add -ggdb to the build" OFF)
option(AUTO_CONVERT_EVENT "Auto convert event to standard event" ON)
option(PG_PROFILE "Enable the profiling of systems" OFF)
option(ENABLE_TIME_TRACE "Add -ftime-trace to Clang builds" OFF)
option(BUILD_STATIC_LIB "Build static library only" OFF)
option(BUILD_EXAMPLES "Build example applications" ON)
option(USE_CCACHE "Use ccache to speed up rebuilds" ON)
option(SKIP_COMPONENT_GENERATION "Skip regeneration of component files (use existing generated files)" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Enable ccache if available and requested
if(USE_CCACHE)
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
        message(STATUS "Enabling ccache to speed up rebuilds")
        set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")

        # ccache + GCC/MinGW PCH need extra sloppiness, otherwise PCH-built TUs
        # always miss the cache. These settings are safe for normal builds and
        # are required for the PCH on ColumbaEngine to actually pay off.
        # See: https://ccache.dev/manual/latest.html#_precompiled_headers
        set(ENV{CCACHE_SLOPPINESS} "pch_defines,time_macros,include_file_mtime,include_file_ctime")
    else()
        message(WARNING "ccache requested but not found - install with: sudo apt install ccache")
    endif()
endif()

# Dependency settings
set(TF_BUILD_EXAMPLES OFF)
set(TF_BUILD_TESTS OFF)
set(TF_BUILD_BENCHMARKS OFF)
set(SDL2MIXER_OPUS OFF)
set(SDL2MIXER_FLAC_LIBFLAC OFF)
set(SDL2MIXER_MOD_MODPLUG OFF)
set(SDL2MIXER_MIDI_FLUIDSYNTH OFF)
set(BUILD_SHARED_LIBS OFF)
set(SDL2_DISABLE_INSTALL ON)
set(SDL_TEST OFF)

# Dependency paths
set(SDL2_DIR "import/SDL2-2.28.5")
set(SDL2MIXER_DIR "import/SDL2_mixer-2.6.3")
set(SDL2NET_DIR "import/SDL2_net-2.2.0")
set(GLM_DIR "import/glm")
set(TASKFLOW_DIR "import/taskflow-3.6.0")
set(GTEST_DIR "import/googletest-1.14.0")
set(GLEW_DIR "import/glew-cmake")
set(TTF_DIR "import/freetype-2.13.3")
set(GLTF_DIR "import/tinygltf")

# Find system dependencies
find_package(OpenGL REQUIRED)

# Emscripten-specific setup
if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    # Set flags separately to avoid concatenation issues
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sUSE_SDL=2 -sUSE_SDL_MIXER=2 -sUSE_SDL_NET=2 -sUSE_FREETYPE=1 -fwasm-exceptions -sUSE_PTHREADS=1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -sUSE_SDL=2 -sUSE_SDL_MIXER=2 -sUSE_SDL_NET=2 -sUSE_FREETYPE=1 -fwasm-exceptions -sUSE_PTHREADS=1")

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O3 -sUSE_SDL=2 -sUSE_SDL_MIXER=2 -sUSE_SDL_NET=2 -sUSE_FREETYPE=1 -fwasm-exceptions -sUSE_PTHREADS=1 --use-preload-plugins \
        --preload-file res \
        --preload-file scripts \
        --preload-file shader \
        --preload-file res/setupRenderer.pg \
        --preload-file res/sysRegister.pg \
        --preload-file res/logManager.pg")

    set(CMAKE_EXECUTABLE_SUFFIX .html)
else()
    # Add all subdirectories for native builds
    add_subdirectory(${SDL2_DIR})
    add_subdirectory(${SDL2MIXER_DIR})
    add_subdirectory(${SDL2NET_DIR})
    add_subdirectory(${GLEW_DIR})
    add_subdirectory(${TTF_DIR})
    add_subdirectory(${GLTF_DIR})
endif()

# Always add GLM (header-only)
add_subdirectory(${GLM_DIR})

# Compiler flags
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    add_compile_options(-DGLEW_STATIC)

    if(${CMAKE_HOST_SYSTEM} MATCHES "Linux")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wnon-virtual-dtor -Woverloaded-virtual -Wfloat-equal -pedantic -pthread")
        # set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wnon-virtual-dtor -Woverloaded-virtual -Wfloat-equal -pedantic -pthread -O3")

        if (${ENABLE_TIME_TRACE} AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftime-trace")
        endif()

        # Detect WSL
        execute_process(COMMAND grep -qi microsoft /proc/version RESULT_VARIABLE IS_WSL)
        if(IS_WSL EQUAL 0)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPG_WSL")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPG_WSL")
        endif()
    endif()

    if(${CMAKE_HOST_SYSTEM} MATCHES "Windows")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread -Wa,-mbig-obj -s")
    endif()
endif()

if(USE_GDB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb")
    message(STATUS, "GDB symbols ENABLED")
else()
    message(STATUS, "GDB symbols DISABLED")
endif()

if(AUTO_CONVERT_EVENT)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPG_AUTO_CONVERT_EVENTS_TO_STANDARD")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPG_AUTO_CONVERT_EVENTS_TO_STANDARD")
    message(STATUS "Auto-conversion of C++ events to StandardEvents is ENABLED")
else()
    message(STATUS "Auto-conversion of C++ events to StandardEvents is DISABLED")
endif()

if(PG_PROFILE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPROFILE")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPROFILE")
    message(STATUS "Profiling of systems is ENABLED")
else()
    message(STATUS "Profiling of systems is DISABLED")
endif()

# Apply build-type specific flags directly to CMAKE_CXX_FLAGS
if(CMAKE_BUILD_TYPE MATCHES Debug)
    if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -DDEBUG")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -DDEBUG")
    else()
        # set(CMAKE_CXX_FLAGS_DEBUG "-Wall -Wextra -g -pthread -Wa,-mbig-obj -O0 -ggdb -DDEBUG")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O1 -DDEBUG")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O1 -DDEBUG")
    endif()
elseif(CMAKE_BUILD_TYPE MATCHES Release)
    if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -DNDEBUG")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -DNDEBUG")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
    endif()
endif()

message("Current c++ debug flags used: '${CMAKE_C_FLAGS}'")
message("Current c++ release flags used: '${CMAKE_CXX_FLAGS}'")

# Minimal engine source files (for bootstrap compiler only)
# These files are needed to run PG scripts (VM + modules + ECS for module registration)
set(MINIMAL_ENGINESOURCE
    # Core compiler/VM files
    src/Engine/Compiler/compiler_debug.cpp
    src/Engine/Compiler/vm_core.cpp
    src/Engine/Compiler/vm_helpers.cpp
    src/Engine/Compiler/vm_binary_op.cpp
    src/Engine/Compiler/vm_struct_op.cpp
    src/Engine/Compiler/vm_profiler.cpp
    src/Engine/Compiler/compiler.cpp
    src/Engine/Compiler/decoded_chunk.cpp
    src/Engine/Compiler/object.cpp
    src/Engine/Compiler/cparser.cpp
    src/Engine/Compiler/bytecode_pass.cpp
    src/Engine/Compiler/bytecode_rewriter.cpp
    src/Engine/Compiler/chunk_serializer.cpp
    src/Engine/Compiler/bytecode_pass_module.cpp
    src/Engine/Compiler/pass/basic_operator_local_indexing.cpp
    src/Engine/Compiler/pass/constant_folding.cpp
    src/Engine/Compiler/pass/long_jump_optimization_pass.cpp
    src/Engine/Compiler/pass/popping_jump_pass.cpp
    src/Engine/Compiler/ecsserialization.cpp
    # Minimal interpreter (only lexer and token for parsing)
    src/Engine/Interpreter/lexer.cpp
    src/Engine/Interpreter/token.cpp
    # ECS system (needed for module registration and VM integration)
    src/Engine/ECS/commanddispatcher.cpp
    src/Engine/ECS/componentregistry.cpp
    src/Engine/ECS/entity.cpp
    src/Engine/ECS/entitysystem.cpp
    src/Engine/ECS/entitysystem_minimal.cpp
    src/Engine/ECS/group.cpp
    src/Engine/ECS/savemanager.cpp
    src/Engine/ECS/sparseset.cpp
    src/Engine/ECS/standardsystem.cpp
    src/Engine/ECS/system.cpp
    src/Engine/ECS/uniqueid.cpp
    # Essential support files
    src/Engine/logger.cpp
    src/Engine/serialization.cpp
    src/Engine/Files/filemanager.cpp
    src/Engine/Files/fileparser.cpp
    src/Engine/Memory/elementtype.cpp
    src/Engine/Memory/parallelfor.cpp
    src/Engine/Profiler/profiler.cpp
    # Core systems (provides EntityName component)
    src/Engine/Systems/coresystems.cpp
    # Versioning
    src/Engine/Versioning/manifest.cpp
    src/Engine/Versioning/versionmanager.cpp
)

# Full engine source files
set(ENGINESOURCE
    src/Engine/engine.cpp
    src/Engine/window.cpp
    src/Engine/configuration.cpp
    src/Engine/Versioning/manifest.cpp
    src/Engine/Versioning/versionmanager.cpp
    src/Engine/logger.cpp
    src/Engine/serialization.cpp
    src/Engine/2D/animator2d.cpp
    src/Engine/2D/camera2d.cpp
    src/Engine/2D/collisionsystem.cpp
    src/Engine/2D/position.cpp
    src/Engine/2D/simple2dobject.cpp
    src/Engine/2D/texture.cpp
    src/Engine/Audio/audiosystem.cpp
    src/Engine/Compiler/compiler_debug.cpp
    src/Engine/Compiler/vm_core.cpp
    src/Engine/Compiler/vm_helpers.cpp
    src/Engine/Compiler/vm_binary_op.cpp
    src/Engine/Compiler/vm_struct_op.cpp
    src/Engine/Compiler/vm_profiler.cpp
    src/Engine/Compiler/compiler.cpp
    src/Engine/Compiler/decoded_chunk.cpp
    src/Engine/Compiler/ecsserialization.cpp
    src/Engine/Compiler/object.cpp
    src/Engine/Compiler/cparser.cpp
    src/Engine/Compiler/bytecode_pass.cpp
    src/Engine/Compiler/bytecode_rewriter.cpp
    src/Engine/Compiler/chunk_serializer.cpp
    src/Engine/Compiler/bytecode_pass_module.cpp
    src/Engine/Compiler/pass/basic_operator_local_indexing.cpp
    src/Engine/Compiler/pass/constant_folding.cpp
    src/Engine/Compiler/pass/long_jump_optimization_pass.cpp
    src/Engine/Compiler/pass/popping_jump_pass.cpp
    src/Engine/ECS/commanddispatcher.cpp
    src/Engine/ECS/componentregistry.cpp
    src/Engine/ECS/entity.cpp
    src/Engine/ECS/entitysystem.cpp
    src/Engine/ECS/entitysystem_full.cpp
    src/Engine/ECS/group.cpp
    src/Engine/ECS/savemanager.cpp
    src/Engine/ECS/sparseset.cpp
    src/Engine/ECS/standardsystem.cpp
    src/Engine/ECS/system.cpp
    src/Engine/ECS/uniqueid.cpp
    src/Engine/Event/eventloop.cpp
    src/Engine/Files/filemanager.cpp
    src/Engine/Files/fileparser.cpp
    src/Engine/Profiler/profiler.cpp
    src/Engine/Init/coresystems.cpp
    src/Engine/Init/rendersystems.cpp
    src/Engine/Init/inputsystems.cpp
    src/Engine/Init/uisystems.cpp
    src/Engine/Init/audiosystems.cpp
    src/Engine/Helpers/fsm.cpp
    src/Engine/Helpers/openglobject.cpp
    src/Engine/Helpers/tinyfiledialogs.cpp
    src/Engine/Input/input.cpp
    src/Engine/Input/inputcomponent.cpp
    src/Engine/Interpreter/environment.cpp
    src/Engine/Interpreter/expression.cpp
    src/Engine/Interpreter/interpreter.cpp
    src/Engine/Interpreter/interpretersystem.cpp
    src/Engine/Interpreter/lexer.cpp
    src/Engine/Interpreter/parser.cpp
    src/Engine/Interpreter/pginterpreter.cpp
    src/Engine/Interpreter/resolver.cpp
    src/Engine/Interpreter/statement.cpp
    src/Engine/Interpreter/systemfunction.cpp
    src/Engine/Interpreter/token.cpp
    src/Engine/Interpreter/valuable.cpp
    src/Engine/Loaders/atlasloader.cpp
    src/Engine/Loaders/Aseprite/asepritefile.cpp
    src/Engine/Loaders/Aseprite/asepritefileatlasloader.cpp
    src/Engine/Loaders/Aseprite/asepriteloader.cpp
    src/Engine/Maths/noise.cpp
    src/Engine/Maths/randomnumbergenerator.cpp
    src/Engine/Memory/elementtype.cpp
    src/Engine/Memory/parallelfor.cpp
    src/Engine/Networking/backend_sdl.cpp
    src/Engine/Networking/common.cpp
    src/Engine/Networking/network_system.cpp
    src/Engine/Renderer/mesh.cpp
    src/Engine/Renderer/particle.cpp
    src/Engine/Renderer/renderer.cpp
    src/Engine/Renderer/camera.cpp
    src/Engine/Scene/scenemanager.cpp
    src/Engine/Shader/shader.cpp
    src/Engine/Systems/coresystems.cpp
    src/Engine/Systems/oneventcomponent.cpp
    src/Engine/Systems/positionmodule.cpp
    src/Engine/Systems/uimodule.cpp
    src/Engine/UI/button.cpp
    src/Engine/UI/focusable.cpp
    src/Engine/UI/listview.cpp
    src/Engine/UI/namedanchor.cpp
    src/Engine/UI/progressbar.cpp
    src/Engine/UI/scrollable.cpp
    src/Engine/UI/sentencesystem.cpp
    src/Engine/UI/sizer.cpp
    src/Engine/UI/textinput.cpp
    src/Engine/UI/thememanager.cpp
    src/Engine/UI/ttftext.cpp
    src/Engine/UI/uianimation.cpp
    src/Engine/UI/uiconstant.cpp
    src/Engine/UI/uisystem.cpp
)

# ===============================================
# COMPONENT GENERATION SETUP (Two-stage build)
# ===============================================

# Find all .pgcomp component definition files
file(GLOB COMPONENT_DEFINITIONS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/Components/*.pgcomp"
)

# Note: ColumbaEngine library will be created AFTER component generation

# ===============================================
# MINIMAL ENGINE LIBRARY + BOOTSTRAP COMPILER
# (native builds only - cannot run wasm tools on host)
# ===============================================
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    # Create a minimal engine library with only VM + modules + ECS
    # This is used ONLY for building the bootstrap compiler
    add_library(ColumbaEngineMinimal STATIC ${MINIMAL_ENGINESOURCE})

    # Disable PCH for minimal engine to avoid rebuilding it
    # (The minimal engine is small enough that PCH overhead isn't worth it,
    #  and it would need a separate PCH due to PG_MINIMAL_BUILD definition)
    set_target_properties(ColumbaEngineMinimal PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)

    # Define PG_MINIMAL_BUILD for the minimal engine
    target_compile_definitions(ColumbaEngineMinimal PRIVATE PG_MINIMAL_BUILD)

    message(STATUS "Created minimal engine library")

    # Minimal engine needs minimal include directories (no SDL2, no rendering libs)
    target_include_directories(ColumbaEngineMinimal PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/Engine>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${GLM_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${TASKFLOW_DIR}>
    )

    # Minimal engine linking (much simpler - no rendering, no audio, no networking)
    # For minimal builds, we don't need SDL2 at all - just GLM for math types
    target_link_libraries(ColumbaEngineMinimal PUBLIC glm)

    # ===============================================
    # BOOTSTRAP COMPILER EXECUTABLE
    # ===============================================
    # Create a minimal PgCompiler executable that can run component generator
    # This uses the minimal engine library to avoid circular dependencies
    add_executable(PgCompilerBootstrap
        examples/PgCompiler/main.cpp
        examples/PgCompiler/application.cpp
        examples/PgCompiler/constant_uniformity_pass.cpp
    )

    target_compile_definitions(PgCompilerBootstrap PRIVATE PG_MINIMAL_BUILD SDL_MAIN_HANDLED)

    target_include_directories(PgCompilerBootstrap PUBLIC examples/PgCompiler)

    # Link against minimal engine (no rendering, no generated components)
    target_link_libraries(PgCompilerBootstrap PRIVATE ColumbaEngineMinimal)

    message(STATUS "Bootstrap compiler 'PgCompilerBootstrap' created (links to minimal engine)")

    # ===============================================
    # COMPONENT REGENERATION TARGET
    # ===============================================
    # Create a target to regenerate components when .pgcomp files change
    # This uses the bootstrap compiler to avoid circular dependencies
    # Output generated files into the build directory instead of the source tree
    set(GENERATED_COMPONENTS_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/Components")
    file(MAKE_DIRECTORY ${GENERATED_COMPONENTS_DIR})

    if(SKIP_COMPONENT_GENERATION)
        message(STATUS "SKIP_COMPONENT_GENERATION is ON: skipping component regeneration (using existing generated files)")
    elseif(COMPONENT_DEFINITIONS)
        set(ALL_GENERATED_FILES "")

        foreach(COMP_DEF ${COMPONENT_DEFINITIONS})
            get_filename_component(COMP_NAME ${COMP_DEF} NAME_WE)

            # Define the output files (in build directory)
            set(GENERATED_HEADER "${GENERATED_COMPONENTS_DIR}/${COMP_NAME}.generated.h")
            set(GENERATED_CPP "${GENERATED_COMPONENTS_DIR}/${COMP_NAME}.generated.cpp")
            set(GENERATED_SER "${GENERATED_COMPONENTS_DIR}/${COMP_NAME}.serialization.cpp")

            # Add custom command that only runs when inputs change
            add_custom_command(
                OUTPUT ${GENERATED_HEADER} ${GENERATED_CPP} ${GENERATED_SER}
                COMMAND ${CMAKE_COMMAND} -E echo "Regenerating ${COMP_NAME}..."
                COMMAND $<TARGET_FILE:PgCompilerBootstrap>
                        ${CMAKE_CURRENT_SOURCE_DIR}/tools/component_generator.pg
                        ${COMP_DEF}
                DEPENDS
                    PgCompilerBootstrap
                    ${COMP_DEF}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tools/component_generator.pg
                WORKING_DIRECTORY ${GENERATED_COMPONENTS_DIR}
                COMMENT "Regenerating ${COMP_NAME} from ${COMP_DEF}"
                VERBATIM
            )

            # Create a target for this component (for manual regeneration)
            add_custom_target(Regenerate_${COMP_NAME}
                DEPENDS ${GENERATED_HEADER} ${GENERATED_CPP} ${GENERATED_SER}
            )

            list(APPEND ALL_GENERATED_FILES ${GENERATED_HEADER} ${GENERATED_CPP} ${GENERATED_SER})

            # Mark the generated files as GENERATED so CMake knows they'll be created
            set_source_files_properties(
                ${GENERATED_HEADER} ${GENERATED_CPP} ${GENERATED_SER}
                PROPERTIES GENERATED TRUE
            )
        endforeach()

        # Create a target to regenerate all components (for manual use)
        add_custom_target(RegenerateComponents
            DEPENDS ${ALL_GENERATED_FILES}
            COMMENT "All components up to date"
        )

        message(STATUS "Component regeneration configured for ${NUM_DEFINITIONS} components")
        message(STATUS "  Components will regenerate only when .pgcomp files change")
        message(STATUS "  Manual regeneration: cmake --build . --target RegenerateComponents")
    endif()
else()
    message(STATUS "Emscripten build: skipping bootstrap compiler and component regeneration (using pre-generated files)")
endif()

# ===============================================
# FULL ENGINE LIBRARY (with generated components)
# ===============================================
# Build the list of generated component sources
set(GENERATED_COMPONENT_SOURCES "")
if(COMPONENT_DEFINITIONS)
    foreach(COMP_DEF ${COMPONENT_DEFINITIONS})
        get_filename_component(COMP_NAME ${COMP_DEF} NAME_WE)
        list(APPEND GENERATED_COMPONENT_SOURCES
            "${GENERATED_COMPONENTS_DIR}/${COMP_NAME}.generated.cpp"
            "${GENERATED_COMPONENTS_DIR}/${COMP_NAME}.serialization.cpp"
        )
    endforeach()

    list(LENGTH GENERATED_COMPONENT_SOURCES NUM_GEN_SOURCES)
    message(STATUS "ColumbaEngine will include ${NUM_GEN_SOURCES} generated component source files")
endif()

# Create the full engine library with generated components
add_library(ColumbaEngine STATIC ${ENGINESOURCE} ${GENERATED_COMPONENT_SOURCES})

# Set precompiled headers - but don't make them public for consumers
# PCH is enabled on all platforms including MinGW. If you hit PCH-related
# build errors on MinGW, the most common cause is ccache: make sure
# CCACHE_SLOPPINESS includes pch_defines,time_macros (configured above).
target_precompile_headers(ColumbaEngine PRIVATE src/Engine/stdafx.h)

# # Unity build: batch source files together to share template instantiations
# # and reduce redundant header parsing. Combined with PCH this is the biggest
# # clean win for full-rebuild times.
# #
# # Batch size note: this engine has very heavy ECS templates (entitysystem.h,
# # System<...> instantiations, taskflow). On 32-bit MinGW, cc1plus has a
# # ~2 GB address-space limit and silently dies when a unity TU instantiates
# # too many template specializations at once. Keep the batch size small —
# # 4 is a safe value that still gives most of the unity-build benefit.
# # 64-bit toolchains (msys2 mingw-w64, clang) can handle much larger batches.
# #
# # If you hit anonymous-namespace symbol clashes (e.g. multiple
# # `static const char* DOM` in the same batch), either rename the offending
# # symbol to be file-unique or set UNITY_BUILD_MODE to GROUP and audit the
# # groupings manually.
# set_target_properties(ColumbaEngine PROPERTIES
#     UNITY_BUILD ON
#     UNITY_BUILD_BATCH_SIZE 4
# )

# Make ColumbaEngine depend on component regeneration (native builds only)
if(COMPONENT_DEFINITIONS AND NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten" AND NOT SKIP_COMPONENT_GENERATION)
    add_dependencies(ColumbaEngine RegenerateComponents)
endif()

# Include directories - ONLY use generator expressions for proper build/install interface
target_include_directories(ColumbaEngine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/Engine>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/GameElements>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${GLM_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${TASKFLOW_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${GLTF_DIR}/include>
    # Generated components in build directory (resolves "Components/X.generated.h" includes)
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
    $<BUILD_INTERFACE:${GENERATED_COMPONENTS_DIR}>
    $<INSTALL_INTERFACE:include/ColumbaEngine>
    $<INSTALL_INTERFACE:include/ColumbaEngine/GameElements>
    $<INSTALL_INTERFACE:include/glm>
    $<INSTALL_INTERFACE:include/taskflow>
    $<INSTALL_INTERFACE:include>
)

# Add platform-specific build interface includes
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    target_include_directories(ColumbaEngine PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${SDL2_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${SDL2MIXER_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${SDL2NET_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${GLEW_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${TTF_DIR}/include>
    )
endif()

# Platform-specific linking - Keep dependencies PUBLIC since we're exporting them
if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    # Emscripten - link publicly so they get exported
    target_link_libraries(ColumbaEngine PUBLIC glm)

    target_compile_options(ColumbaEngine PRIVATE ${USE_FLAGS})
else()
    # Native platforms - link publicly so they get exported
    if (TARGET SDL2::SDL2main)
        target_link_libraries(ColumbaEngine PUBLIC SDL2::SDL2main)
    endif()

    target_link_options(ColumbaEngine PRIVATE -static-libgcc -static-libstdc++)

    # Check for target name variations and use the correct ones
    set(SDL2_TARGETS)
    if(TARGET SDL2::SDL2-static)
        list(APPEND SDL2_TARGETS SDL2::SDL2-static)
    elseif(TARGET SDL2-static)
        list(APPEND SDL2_TARGETS SDL2-static)
    endif()

    # Handle SDL2_mixer target name variations - use the actual target name, not alias
    if(TARGET SDL2_mixer)
        list(APPEND SDL2_TARGETS SDL2_mixer)
    elseif(TARGET SDL2_mixer::SDL2_mixer-static)
        list(APPEND SDL2_TARGETS SDL2_mixer::SDL2_mixer-static)
    elseif(TARGET SDL2_mixer-static)
        list(APPEND SDL2_TARGETS SDL2_mixer-static)
    endif()

    # Handle SDL2_net target name variations - use the actual target name
    if(TARGET SDL2_net)
        list(APPEND SDL2_TARGETS SDL2_net)
    elseif(TARGET SDL2_net::SDL2_net-static)
        list(APPEND SDL2_TARGETS SDL2_net::SDL2_net-static)
    elseif(TARGET SDL2_net-static)
        list(APPEND SDL2_TARGETS SDL2_net-static)
    endif()

    message(STATUS "SDL2 targets to link: ${SDL2_TARGETS}")

    target_link_libraries(ColumbaEngine PUBLIC
        ${SDL2_TARGETS}
        glm
        libglew_static
        freetype
        OpenGL::GL
    )
endif()

# Debug: Print all available targets
get_property(ALL_TARGETS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
message(STATUS "All available targets: ${ALL_TARGETS}")

# Check specific targets and their actual names
foreach(target IN ITEMS SDL2main SDL2-static SDL2_mixer-static SDL2_mixer SDL2_net glm libglew_static freetype)
    if(TARGET ${target})
        message(STATUS "Target ${target} exists")
    else()
        message(STATUS "Target ${target} does NOT exist")
    endif()
endforeach()

# Also check for SDL2_mixer target variations
foreach(target IN ITEMS "SDL2_mixer::SDL2_mixer-static" "SDL2_mixer::SDL2_mixer" "SDL2_mixer-static" "SDL2_mixer")
    if(TARGET ${target})
        message(STATUS "SDL2_mixer target ${target} exists")
    else()
        message(STATUS "SDL2_mixer target ${target} does NOT exist")
    endif()
endforeach()

# Installation - Install ColumbaEngine and all its static dependencies
if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    # Emscripten build - only install ColumbaEngine and glm
    install(TARGETS ColumbaEngine glm
        EXPORT ColumbaEngineTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
    )
else()
    # Native build - install all static dependencies
    # First check which targets actually exist
    set(TARGETS_TO_INSTALL ColumbaEngine)

    # Add targets that exist (check for both alias and direct names)
    if(TARGET SDL2-static)
        list(APPEND TARGETS_TO_INSTALL SDL2-static)
    endif()

    if(TARGET SDL2main)
        list(APPEND TARGETS_TO_INSTALL SDL2main)
    endif()

    if(TARGET SDL2_mixer-static)
        list(APPEND TARGETS_TO_INSTALL SDL2_mixer-static)
    elseif(TARGET SDL2_mixer)
        list(APPEND TARGETS_TO_INSTALL SDL2_mixer)
    endif()

    if(TARGET SDL2_net-static)
        list(APPEND TARGETS_TO_INSTALL SDL2_net-static)
    elseif(TARGET SDL2_net)
        list(APPEND TARGETS_TO_INSTALL SDL2_net)
    endif()

    if(TARGET glm)
        list(APPEND TARGETS_TO_INSTALL glm)
    endif()

    if(TARGET libglew_static)
        list(APPEND TARGETS_TO_INSTALL libglew_static)
    endif()

    if(TARGET freetype)
        list(APPEND TARGETS_TO_INSTALL freetype)
    endif()

    if(TARGET tinygltf)
        list(APPEND TARGETS_TO_INSTALL tinygltf)
    endif()

    # Debug: Print what targets we're installing
    message(STATUS "Installing targets: ${TARGETS_TO_INSTALL}")

    install(TARGETS ${TARGETS_TO_INSTALL}
        EXPORT ColumbaEngineTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
    )
endif()

# Install ALL headers including dependencies
install(DIRECTORY src/Engine/
    DESTINATION include/ColumbaEngine
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

install(DIRECTORY src/GameElements/
    DESTINATION include/ColumbaEngine/GameElements
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Export targets - this creates the ColumbaEngineTargets.cmake file
install(EXPORT ColumbaEngineTargets
    FILE ColumbaEngineTargets.cmake
    NAMESPACE ColumbaEngine::
    DESTINATION lib/cmake/ColumbaEngine
)

# Install vendored dependencies headers
install(DIRECTORY ${GLM_DIR}/glm/
    DESTINATION include/glm
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h" PATTERN "*.inl"
)

install(DIRECTORY ${TASKFLOW_DIR}/taskflow/
    DESTINATION include/taskflow
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

install(DIRECTORY ${GLTF_DIR}/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

# Install platform-specific headers for native builds
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    # Install SDL2 headers
    install(DIRECTORY ${SDL2_DIR}/include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
    )

    # Install SDL2_mixer headers - be explicit about the path
    install(FILES ${SDL2MIXER_DIR}/include/SDL_mixer.h
        DESTINATION include
    )

    # Install SDL2_net headers
    install(DIRECTORY ${SDL2NET_DIR}/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
        PATTERN "CMakeLists.txt" EXCLUDE
        PATTERN "*.c" EXCLUDE
        PATTERN "*.cpp" EXCLUDE
    )

    # Install GLEW headers
    install(DIRECTORY ${GLEW_DIR}/include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
    )

    # Install FreeType headers
    install(DIRECTORY ${TTF_DIR}/include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
    )
endif()

# Create package config files
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ColumbaEngineConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ColumbaEngineConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ColumbaEngineConfig.cmake"
    INSTALL_DESTINATION lib/cmake/ColumbaEngine
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ColumbaEngineConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ColumbaEngineConfigVersion.cmake"
    DESTINATION lib/cmake/ColumbaEngine
)

# Only build examples if not building static lib only
if(NOT BUILD_STATIC_LIB AND BUILD_EXAMPLES)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
        add_executable(GMTK2025 examples/GMTK2025/main.cpp)

        target_include_directories(GMTK2025 PUBLIC examples/GMTK2025)

        target_sources(GMTK2025 PRIVATE
            examples/GMTK2025/application.cpp
            examples/GMTK2025/ribbonmesh.cpp
            examples/GMTK2025/polygonmesh.cpp
        )

        target_link_libraries(GMTK2025 PRIVATE ColumbaEngine)

        set_target_properties(GMTK2025
            PROPERTIES SUFFIX ".html"
            LINK_FLAGS "-sWASMFS -s FORCE_FILESYSTEM=1 -sPTHREAD_POOL_SIZE=4 -pthread -s ALLOW_MEMORY_GROWTH=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s SAFE_HEAP=1 ${USE_FLAGS}")

        ###

        add_executable(BoxBouncer examples/SimpleBoxBouncer/main.cpp)

        target_include_directories(BoxBouncer PUBLIC examples/SimpleBoxBouncer)

        target_sources(BoxBouncer PRIVATE
            examples/SimpleBoxBouncer/application.cpp
        )

        target_link_libraries(BoxBouncer PRIVATE ColumbaEngine)

        set_target_properties(BoxBouncer
            PROPERTIES SUFFIX ".html"
            LINK_FLAGS "-sWASMFS -s FORCE_FILESYSTEM=1 -sPTHREAD_POOL_SIZE=4 -pthread -s ALLOW_MEMORY_GROWTH=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s SAFE_HEAP=1 ${USE_FLAGS}")

        ###

        add_executable(RenderingTest examples/RenderingTest/main.cpp)

        target_include_directories(RenderingTest PUBLIC examples/RenderingTest)

        target_sources(RenderingTest PRIVATE
            examples/RenderingTest/application.cpp
        )

        target_link_libraries(RenderingTest PRIVATE ColumbaEngine)

        set_target_properties(RenderingTest
            PROPERTIES SUFFIX ".html"
            LINK_FLAGS "-sWASMFS -s FORCE_FILESYSTEM=1 -sPTHREAD_POOL_SIZE=4 -pthread -s ALLOW_MEMORY_GROWTH=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s SAFE_HEAP=1 ${USE_FLAGS}")

        ###

        add_executable(Asteroid examples/Asteroid/main.cpp)

        target_include_directories(Asteroid PUBLIC examples/Asteroid)

        target_sources(Asteroid PRIVATE
            examples/Asteroid/application.cpp
        )

        target_link_libraries(Asteroid PRIVATE ColumbaEngine)

        set_target_properties(Asteroid
            PROPERTIES SUFFIX ".html"
            LINK_FLAGS "-sWASMFS -s FORCE_FILESYSTEM=1 -sPTHREAD_POOL_SIZE=4 -pthread -s ALLOW_MEMORY_GROWTH=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s SAFE_HEAP=1 ${USE_FLAGS}")

        ###

        add_executable(InvadersBreaker examples/InvadersBreaker/main.cpp)

        target_include_directories(InvadersBreaker PUBLIC examples/InvadersBreaker)

        target_sources(InvadersBreaker PRIVATE
            examples/InvadersBreaker/application.cpp
        )

        target_link_libraries(InvadersBreaker PRIVATE ColumbaEngine)

        set_target_properties(InvadersBreaker
            PROPERTIES SUFFIX ".html"
            LINK_FLAGS "-sWASMFS -s FORCE_FILESYSTEM=1 -sPTHREAD_POOL_SIZE=4 -pthread -s ALLOW_MEMORY_GROWTH=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s SAFE_HEAP=1 ${USE_FLAGS}")

        ###

        add_executable(VoxelTest examples/VoxelTest/main.cpp)

        target_include_directories(VoxelTest PUBLIC examples/VoxelTest)

        target_sources(VoxelTest PRIVATE
            examples/VoxelTest/application.cpp
            examples/VoxelTest/cubemesh.cpp
            examples/VoxelTest/voxelrenderer.cpp
            examples/VoxelTest/camera3dcontroller.cpp
            examples/VoxelTest/camera3dcontrollereditor.cpp
            examples/VoxelTest/editorsystem.cpp
            examples/VoxelTest/editorui.cpp
            examples/VoxelTest/voxelserializer.cpp
        )

        target_link_libraries(VoxelTest PRIVATE ColumbaEngine)

        set_target_properties(VoxelTest
            PROPERTIES SUFFIX ".html"
            LINK_FLAGS "-sWASMFS -s FORCE_FILESYSTEM=1 -sPTHREAD_POOL_SIZE=4 -pthread -s ALLOW_MEMORY_GROWTH=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s SAFE_HEAP=1 ${USE_FLAGS}")

        ###

        add_executable(GameDevJs2026 examples/GameDevJs2026/main.cpp)

        target_include_directories(GameDevJs2026 PUBLIC
            examples/GameDevJs2026
            examples/GameDevJs2026/Core
            examples/GameDevJs2026/Registries
            examples/GameDevJs2026/Systems
            examples/GameDevJs2026/UI
        )

        target_sources(GameDevJs2026 PRIVATE
            examples/GameDevJs2026/application.cpp
            examples/GameDevJs2026/Core/gridatlas.cpp
            examples/GameDevJs2026/Core/canvasgenerator.cpp
            examples/GameDevJs2026/Core/worldfacts.cpp
            examples/GameDevJs2026/Core/saveserialization.cpp
            examples/GameDevJs2026/Registries/inventory.cpp
            examples/GameDevJs2026/Registries/itemregistry.cpp
            examples/GameDevJs2026/Registries/buildingregistry.cpp
            examples/GameDevJs2026/Registries/reciperegistry.cpp
            examples/GameDevJs2026/Registries/missionregistry.cpp
            examples/GameDevJs2026/Systems/camerasystem.cpp
            examples/GameDevJs2026/Systems/gridsystem.cpp
            examples/GameDevJs2026/Systems/gamesystem.cpp
            examples/GameDevJs2026/Systems/minersystem.cpp
            examples/GameDevJs2026/Systems/transportsystem.cpp
            examples/GameDevJs2026/Systems/insertersystem.cpp
            examples/GameDevJs2026/Systems/craftingsystem.cpp
            examples/GameDevJs2026/Systems/storagesystem.cpp
            examples/GameDevJs2026/Systems/handcraftingsystem.cpp
            examples/GameDevJs2026/Systems/playerinventory.cpp
            examples/GameDevJs2026/Systems/manualmining.cpp
            examples/GameDevJs2026/Systems/depotsystem.cpp
            examples/GameDevJs2026/UI/hotbarsystem.cpp
            examples/GameDevJs2026/UI/inventoryui.cpp
            examples/GameDevJs2026/UI/minerui.cpp
            examples/GameDevJs2026/UI/craftingui.cpp
            examples/GameDevJs2026/UI/machineui.cpp
            examples/GameDevJs2026/UI/storageui.cpp
            examples/GameDevJs2026/UI/tooltipsystem.cpp
            examples/GameDevJs2026/UI/tutorialsystem.cpp
            examples/GameDevJs2026/UI/machinedemosystem.cpp
            examples/GameDevJs2026/UI/depotui.cpp
            examples/GameDevJs2026/UI/hudbarsystem.cpp
            examples/GameDevJs2026/Systems/missionsystem.cpp
            examples/GameDevJs2026/Systems/analyticssystem.cpp
            examples/GameDevJs2026/UI/missionui.cpp
        )

        target_link_libraries(GameDevJs2026 PRIVATE ColumbaEngine)

        set_target_properties(GameDevJs2026
            PROPERTIES SUFFIX ".html"
            LINK_FLAGS "-sWASMFS -s FORCE_FILESYSTEM=1 -sPTHREAD_POOL_SIZE=4 -pthread -s ALLOW_MEMORY_GROWTH=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s SAFE_HEAP=1 ${USE_FLAGS}")

        ###

    else()
        add_executable(ColumbaEngineEditor src/main.cpp)

        target_include_directories(ColumbaEngineEditor PUBLIC src/Editor)

        target_sources(ColumbaEngineEditor PRIVATE
            src/application.cpp
            src/application.h
            src/Editor/Gui/contextmenu.cpp
            src/Editor/Gui/inspector.cpp
            src/Editor/Gui/entitylist.cpp
            src/Editor/Gui/projectmanager.cpp
        )

        target_link_libraries(ColumbaEngineEditor PRIVATE ColumbaEngine)

        add_executable(GameOff examples/GameOff/main.cpp)

        # configure_file(res/main.pg nexus.pg COPYONLY)

        target_include_directories(GameOff PUBLIC examples/GameOff)

        target_sources(GameOff PRIVATE
            examples/GameOff/achievement.cpp
            examples/GameOff/application.cpp
            examples/GameOff/character.cpp
            examples/GameOff/characustomizationscene.cpp
            examples/GameOff/fightscene.cpp
            examples/GameOff/gamefacts.cpp
            examples/GameOff/inventory.cpp
            examples/GameOff/item.cpp
            examples/GameOff/location.cpp
            examples/GameOff/locationscene.cpp
            examples/GameOff/mainscene.cpp
            examples/GameOff/managenerator.cpp
            examples/GameOff/nexusbindings.cpp
            examples/GameOff/nexusmodule.cpp
            examples/GameOff/nexusscene.cpp
            examples/GameOff/skilltree.cpp
            examples/GameOff/passives.cpp
            examples/GameOff/spells.cpp
            examples/GameOff/theme.cpp
        )

        target_link_libraries(GameOff PRIVATE ColumbaEngine)

        add_executable(PixelJam examples/PixelJam/main.cpp)

        target_include_directories(PixelJam PUBLIC examples/PixelJam)

        target_sources(PixelJam PRIVATE
            examples/PixelJam/application.cpp
            examples/PixelJam/Characters/enemy.cpp
            examples/PixelJam/Characters/player.cpp
            examples/PixelJam/config.cpp
            examples/PixelJam/Tiled_Lib/TiledLoader.cpp
            examples/PixelJam/Tiled_Lib/TileMapAtlasLoader.cpp
            examples/PixelJam/Room/room.cpp
        )

        target_link_libraries(PixelJam PRIVATE ColumbaEngine)

        add_executable(TugOfWar examples/TugOfWar/main.cpp)

        target_include_directories(TugOfWar PUBLIC examples/TugOfWar)

        target_sources(TugOfWar PRIVATE
            examples/TugOfWar/application.cpp
        )

        target_link_libraries(TugOfWar PRIVATE ColumbaEngine)

        ###

        add_executable(SimpleClientServer examples/SimpleClientServer/main.cpp)

        target_sources(SimpleClientServer PRIVATE
            examples/SimpleClientServer/application.cpp
        )

        target_link_libraries(SimpleClientServer PRIVATE ColumbaEngine)

        ###

        add_executable(BasicTerminal examples/BasicTerminal/main.cpp)

        target_include_directories(BasicTerminal PUBLIC examples/BasicTerminal)

        target_sources(BasicTerminal PRIVATE
            examples/BasicTerminal/application.cpp
        )

        target_link_libraries(BasicTerminal PRIVATE ColumbaEngine)

        ###

        add_executable(EmptyApp examples/EmptyAppTemplate/main.cpp)

        target_include_directories(EmptyApp PUBLIC examples/EmptyAppTemplate)

        target_sources(EmptyApp PRIVATE
            examples/EmptyAppTemplate/application.cpp
        )

        target_link_libraries(EmptyApp PRIVATE ColumbaEngine)

        ###

        add_executable(GMTK2025 examples/GMTK2025/main.cpp)

        target_include_directories(GMTK2025 PUBLIC examples/GMTK2025)

        target_sources(GMTK2025 PRIVATE
            examples/GMTK2025/application.cpp
            examples/GMTK2025/ribbonmesh.cpp
            examples/GMTK2025/polygonmesh.cpp
        )

        target_link_libraries(GMTK2025 PRIVATE ColumbaEngine)

        ###

        add_executable(InvadersBreaker examples/InvadersBreaker/main.cpp)

        target_include_directories(InvadersBreaker PUBLIC examples/InvadersBreaker)

        target_sources(InvadersBreaker PRIVATE
            examples/InvadersBreaker/application.cpp
        )

        target_link_libraries(InvadersBreaker PRIVATE ColumbaEngine)

        ###

        add_executable(BoxBouncer examples/SimpleBoxBouncer/main.cpp)

        target_include_directories(BoxBouncer PUBLIC examples/SimpleBoxBouncer)

        target_sources(BoxBouncer PRIVATE
            examples/SimpleBoxBouncer/application.cpp
        )

        target_link_libraries(BoxBouncer PRIVATE ColumbaEngine)

        ###

        add_executable(RenderingTest examples/RenderingTest/main.cpp)

        target_include_directories(RenderingTest PUBLIC examples/RenderingTest)

        target_sources(RenderingTest PRIVATE
            examples/RenderingTest/application.cpp
        )

        target_link_libraries(RenderingTest PRIVATE ColumbaEngine)

        ###

        add_executable(Asteroid examples/Asteroid/main.cpp)

        target_include_directories(Asteroid PUBLIC examples/Asteroid)

        target_sources(Asteroid PRIVATE
            examples/Asteroid/application.cpp
        )

        target_link_libraries(Asteroid PRIVATE ColumbaEngine)

        ###

        add_executable(StandardSys examples/StandardSys/main.cpp)

        target_include_directories(StandardSys PUBLIC examples/StandardSys)

        target_sources(StandardSys PRIVATE
            examples/StandardSys/application.cpp
        )

        target_link_libraries(StandardSys PRIVATE ColumbaEngine)

        ###

        add_executable(VoxelTest examples/VoxelTest/main.cpp)

        target_include_directories(VoxelTest PUBLIC examples/VoxelTest)

        target_sources(VoxelTest PRIVATE
            examples/VoxelTest/application.cpp
            examples/VoxelTest/cubemesh.cpp
            examples/VoxelTest/voxelrenderer.cpp
            examples/VoxelTest/camera3dcontroller.cpp
            examples/VoxelTest/camera3dcontrollereditor.cpp
            examples/VoxelTest/editorsystem.cpp
            examples/VoxelTest/editorui.cpp
            examples/VoxelTest/voxelserializer.cpp
        )

        target_link_libraries(VoxelTest PRIVATE ColumbaEngine)

        ###

        add_executable(PgInterpreter examples/PgInterpreter/main.cpp)

        target_include_directories(PgInterpreter PUBLIC examples/PgInterpreter)

        target_sources(PgInterpreter PRIVATE
            examples/PgInterpreter/application.cpp
        )

        target_link_libraries(PgInterpreter PRIVATE ColumbaEngine)

        ###

        add_executable(AtlasPicker examples/AtlasPicker/main.cpp)

        target_include_directories(AtlasPicker PUBLIC
            examples/AtlasPicker
            examples/GameDevJs2026/Core
        )

        target_sources(AtlasPicker PRIVATE
            examples/AtlasPicker/application.cpp
            examples/AtlasPicker/atlaspickersystem.cpp
            examples/GameDevJs2026/Core/gridatlas.cpp
        )

        target_link_libraries(AtlasPicker PRIVATE ColumbaEngine)

        ###

        add_executable(PgCompiler examples/PgCompiler/main.cpp)

        target_include_directories(PgCompiler PUBLIC examples/PgCompiler)

        target_sources(PgCompiler PRIVATE
            examples/PgCompiler/application.cpp
            examples/PgCompiler/constant_uniformity_pass.cpp
        )

        target_link_libraries(PgCompiler PRIVATE ColumbaEngine)

        # ===============================================
        # COMPONENT GENERATION BUILD STEP
        # ===============================================
        # Populate the GenerateComponents target now that PgCompiler exists
        if(COMPONENT_DEFINITIONS AND TARGET GenerateComponents)
            # Make GenerateComponents depend on PgCompiler being built
            add_dependencies(GenerateComponents PgCompiler)

            foreach(COMP_DEF ${COMPONENT_DEFINITIONS})
                get_filename_component(COMP_NAME ${COMP_DEF} NAME_WE)

                # Expected output files
                set(GENERATED_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/Components/${COMP_NAME}.generated.h")
                set(GENERATED_CPP "${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/Components/${COMP_NAME}.generated.cpp")
                set(GENERATED_SER "${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/Components/${COMP_NAME}.serialization.cpp")

                # Add custom command to generate this component
                add_custom_command(
                    OUTPUT ${GENERATED_HEADER} ${GENERATED_CPP} ${GENERATED_SER}
                    COMMAND ${CMAKE_COMMAND} -E echo "Generating ${COMP_NAME} component..."
                    COMMAND $<TARGET_FILE:PgCompiler>
                            ${CMAKE_CURRENT_SOURCE_DIR}/tools/component_generator.pg
                            ${COMP_DEF}
                    COMMAND ${CMAKE_COMMAND} -E copy
                            ${CMAKE_CURRENT_BINARY_DIR}/${COMP_NAME}.generated.h
                            ${GENERATED_HEADER}
                    COMMAND ${CMAKE_COMMAND} -E copy
                            ${CMAKE_CURRENT_BINARY_DIR}/${COMP_NAME}.generated.cpp
                            ${GENERATED_CPP}
                    COMMAND ${CMAKE_COMMAND} -E copy
                            ${CMAKE_CURRENT_BINARY_DIR}/${COMP_NAME}.serialization.cpp
                            ${GENERATED_SER}
                    DEPENDS PgCompiler ${COMP_DEF} ${CMAKE_CURRENT_SOURCE_DIR}/tools/component_generator.pg
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                    COMMENT "Generating ${COMP_NAME} from ${COMP_DEF}"
                    VERBATIM
                )

                # Add the outputs to the GenerateComponents target
                add_custom_target(Generate_${COMP_NAME}
                    DEPENDS ${GENERATED_HEADER} ${GENERATED_CPP} ${GENERATED_SER}
                )
                add_dependencies(GenerateComponents Generate_${COMP_NAME})
            endforeach()

            message(STATUS "Component generation configured for ${NUM_DEFINITIONS} components")
            message(STATUS "Run 'cmake --build . --target GenerateComponents' to generate components")
        endif()

        ###

        add_executable(EcsSerializationExample examples/PgCompiler/ecs_serialization_example.cpp)

        target_include_directories(EcsSerializationExample PUBLIC examples/PgCompiler)

        target_sources(EcsSerializationExample PRIVATE
        )

        target_link_libraries(EcsSerializationExample PRIVATE ColumbaEngine)

        ###

        add_executable(GameDevJs2026 examples/GameDevJs2026/main.cpp)

        target_include_directories(GameDevJs2026 PUBLIC
            examples/GameDevJs2026
            examples/GameDevJs2026/Core
            examples/GameDevJs2026/Registries
            examples/GameDevJs2026/Systems
            examples/GameDevJs2026/UI
        )

        target_sources(GameDevJs2026 PRIVATE
            examples/GameDevJs2026/application.cpp
            examples/GameDevJs2026/Core/gridatlas.cpp
            examples/GameDevJs2026/Core/canvasgenerator.cpp
            examples/GameDevJs2026/Core/worldfacts.cpp
            examples/GameDevJs2026/Core/saveserialization.cpp
            examples/GameDevJs2026/Registries/inventory.cpp
            examples/GameDevJs2026/Registries/itemregistry.cpp
            examples/GameDevJs2026/Registries/buildingregistry.cpp
            examples/GameDevJs2026/Registries/reciperegistry.cpp
            examples/GameDevJs2026/Registries/missionregistry.cpp
            examples/GameDevJs2026/Systems/camerasystem.cpp
            examples/GameDevJs2026/Systems/gridsystem.cpp
            examples/GameDevJs2026/Systems/gamesystem.cpp
            examples/GameDevJs2026/Systems/minersystem.cpp
            examples/GameDevJs2026/Systems/transportsystem.cpp
            examples/GameDevJs2026/Systems/insertersystem.cpp
            examples/GameDevJs2026/Systems/craftingsystem.cpp
            examples/GameDevJs2026/Systems/storagesystem.cpp
            examples/GameDevJs2026/Systems/handcraftingsystem.cpp
            examples/GameDevJs2026/Systems/playerinventory.cpp
            examples/GameDevJs2026/Systems/manualmining.cpp
            examples/GameDevJs2026/Systems/depotsystem.cpp
            examples/GameDevJs2026/UI/hotbarsystem.cpp
            examples/GameDevJs2026/UI/inventoryui.cpp
            examples/GameDevJs2026/UI/minerui.cpp
            examples/GameDevJs2026/UI/craftingui.cpp
            examples/GameDevJs2026/UI/machineui.cpp
            examples/GameDevJs2026/UI/storageui.cpp
            examples/GameDevJs2026/UI/tooltipsystem.cpp
            examples/GameDevJs2026/UI/tutorialsystem.cpp
            examples/GameDevJs2026/UI/machinedemosystem.cpp
            examples/GameDevJs2026/UI/depotui.cpp
            examples/GameDevJs2026/UI/hudbarsystem.cpp
            examples/GameDevJs2026/Systems/missionsystem.cpp
            examples/GameDevJs2026/Systems/analyticssystem.cpp
            examples/GameDevJs2026/UI/missionui.cpp
        )

        target_link_libraries(GameDevJs2026 PRIVATE ColumbaEngine)

        # Only build tests if not building static lib only
        enable_testing()

        add_subdirectory(${GTEST_DIR})

        include_directories(${GTEST_DIR}/googletest/include)
        include_directories(${GTEST_DIR}/googletest/include/gtest)
        include_directories(${GTEST_DIR}/googletest/include/gtest/internal)

        add_executable(t1 test/maintest.cc)

        target_sources(t1 PRIVATE
            test/mocksentencesystem.h
            # test/sentencesystem.cc
            test/collision2d.cc
            test/ecssystem.cc
            test/filemanager.cc
            test/interpreter.cc
            test/layout.cc
            test/prefab.cc
            test/network.cc
            # test/mock2dsimpleshape.h
            # test/simple2dobject.cc
            test/position.cc
            test/memorypool.cc
            test/mockinterpreter.h
            test/mocklogger.h
            test/mockloggertest.cc
            test/renderer.cc
            test/serialize.cc
            test/taskflow.cc
            test/uiconstanttest.cc
            test/uisystemtest.cc
        )

        target_link_libraries(t1 PRIVATE gtest gtest_main ColumbaEngine)

        add_custom_command(TARGET t1 PRE_BUILD
                           COMMAND ${CMAKE_COMMAND} -E copy_directory
                           ${CMAKE_SOURCE_DIR}/testdeps/ $<TARGET_FILE_DIR:t1>)

        include(GoogleTest)

        gtest_discover_tests(t1
            PROPERTIES
            FAIL_REGULAR_EXPRESSION "FAILED"
        )

        add_executable(bench benchmark/mainbenchmark.cc)

        target_sources(bench PRIVATE
            benchmark/memorypool.cc
            benchmark/serialization.cc
            benchmark/script_performance.cc
            benchmark/ecs_performance.cc
        )

        target_link_libraries(bench PRIVATE gtest gtest_main ColumbaEngine)

        add_custom_command(TARGET bench PRE_BUILD
                           COMMAND ${CMAKE_COMMAND} -E copy_directory
                           ${CMAKE_SOURCE_DIR}/test/bench $<TARGET_FILE_DIR:bench>/test/bench)

        # ===============================================
        # COMPILER TEST TARGET (simple script-based testbench)
        # ===============================================

        # Create the compiler test executable with simple script testbench
        add_executable(test_compiler test/pgcompiler/script_testbench.cc)

        target_sources(test_compiler PRIVATE
            test/pgcompiler/unit/test_vm_memory.cc
            test/pgcompiler/unit/test_register_ops.cc
            test/pgcompiler/unit/test_bytecode_rewriter.cc
        )

        # Include directories for compiler tests
        target_include_directories(test_compiler PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/PgCompiler
        )

        # Link libraries for compiler tests
        target_link_libraries(test_compiler PRIVATE
            gtest
            gtest_main
            ColumbaEngineMinimal
        )

        # Add DEBUG_CHECK_STACK define for test_compiler target
        # target_compile_definitions(test_compiler PRIVATE DEBUG_CHECK_STACK)
        target_compile_definitions(test_compiler PRIVATE IN_PGCOMPILER_TESTS_BUILD)

        # Copy test script files to build directory
        add_custom_command(TARGET test_compiler POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_SOURCE_DIR}/test/pgcompiler/scripts $<TARGET_FILE_DIR:test_compiler>/test/pgcompiler/scripts)

        add_custom_command(TARGET test_compiler POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_SOURCE_DIR}/test/pgcompiler/examples $<TARGET_FILE_DIR:test_compiler>/test/pgcompiler/examples)

        # Register compiler tests with CTest
        gtest_discover_tests(test_compiler
            WORKING_DIRECTORY $<TARGET_FILE_DIR:test_compiler>
            PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:test_compiler>"
        )
    endif()
endif()

# CPack configuration for creating installers
set(CPACK_PACKAGE_NAME "ColumbaEngine")
set(CPACK_PACKAGE_VENDOR "PigeonCodeur")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "ColumbaEngine - A modern C++ game engine")
set(CPACK_PACKAGE_DESCRIPTION "ColumbaEngine is a feature-rich game engine with 2D/3D rendering, ECS architecture, audio system, and scripting capabilities.")
set(CPACK_PACKAGE_CONTACT "pigeoncodeur@gmail.com")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Gallasko/ColumbaEngine")

# DEB package specific settings
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgl1-mesa-dev, libfreetype6-dev")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# RPM package specific settings
set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
set(CPACK_RPM_PACKAGE_LICENSE "MIT") # Adjust based on your license
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

# TGZ archive
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)

# Detect available generators
set(CPACK_GENERATOR "TGZ")

# Check for DEB tools
find_program(DPKG_CMD dpkg)
if(DPKG_CMD)
    list(APPEND CPACK_GENERATOR "DEB")
endif()

# Check for RPM tools
find_program(RPMBUILD_CMD rpmbuild)
if(RPMBUILD_CMD)
    list(APPEND CPACK_GENERATOR "RPM")
endif()

message(STATUS "CPack generators: ${CPACK_GENERATOR}")

include(CPack)
