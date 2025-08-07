# Pure vendoring approach - everything is self-contained
cmake_minimum_required(VERSION 3.18)
project(PgEngine VERSION 1.0)

# Options remain the same
option(UseGDB "UseGDB" OFF)
option(ENABLE_TIME_TRACE "Add -ftime-trace to Clang builds" OFF)
option(BUILD_STATIC_LIB "Build static library only" OFF)
option(BUILD_EXAMPLES "Build example applications" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

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
    set(USE_FLAGS "-O3 -sUSE_SDL=2 -sUSE_SDL_MIXER=2 -sUSE_SDL_NET=2 -sUSE_FREETYPE=1 -fwasm-exceptions -sUSE_PTHREADS=1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${USE_FLAGS} --use-preload-plugins \
        --preload-file res \
        --preload-file scripts \
        --preload-file shader \
        --preload-file setupRenderer.pg \
        --preload-file sysRegister.pg \
        --preload-file logManager.pg")
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
        set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wnon-virtual-dtor -Woverloaded-virtual -Wfloat-equal -pedantic -pthread -O0 -ggdb -DDEBUG")

        if (${ENABLE_TIME_TRACE} AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftime-trace")
        endif()
    endif()

    if(${CMAKE_HOST_SYSTEM} MATCHES "Windows")
        set(CMAKE_CXX_FLAGS " -Wall -Wextra -pthread -Wa,-mbig-obj -s -O2 -DNDEBUG")
    endif()
endif()

if(${UseGDB} EQUAL ON)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb")
endif()

# Debug Flags
if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(CMAKE_C_FLAGS_DEBUG "-Wall -Wextra -g -pthread -ggdb -O0 -DDEBUG ${USE_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "-Wall -Wextra -g -pthread -ggdb -O0 -DDEBUG ${USE_FLAGS}")
else()
    set(CMAKE_CXX_FLAGS_DEBUG "-Wall -Wextra -g -pthread -Wa,-mbig-obj -O0 -ggdb -DDEBUG")
endif()

# Release Flags
if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(CMAKE_C_FLAGS_RELEASE "-Wall -Wextra -pthread -O3 -DNDEBUG -m32 ${USE_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "-Wall -Wextra -pthread -O3 -DNDEBUG -m32 ${USE_FLAGS}")
else()
    # Release flags for native builds can be set here if needed
endif()

message("Current c++ debug flags used: '${CMAKE_CXX_FLAGS_DEBUG}'")
message("Current c++ release flags used: '${CMAKE_CXX_FLAGS_RELEASE}'")

set(ENGINESOURCE
    src/Engine/window.cpp
    src/Engine/configuration.cpp
    src/Engine/logger.cpp
    src/Engine/serialization.cpp
    src/Engine/2D/camera2d.cpp
    src/Engine/2D/collisionsystem.cpp
    src/Engine/2D/position.cpp
    src/Engine/2D/simple2dobject.cpp
    src/Engine/2D/texture.cpp
    src/Engine/Audio/audiosystem.cpp
    src/Engine/ECS/commanddispatcher.cpp
    src/Engine/ECS/componentregistry.cpp
    src/Engine/ECS/entity.cpp
    src/Engine/ECS/entitysystem.cpp
    src/Engine/ECS/group.cpp
    src/Engine/ECS/savemanager.cpp
    src/Engine/ECS/sparseset.cpp
    src/Engine/ECS/system.cpp
    src/Engine/ECS/uniqueid.cpp
    src/Engine/Event/eventloop.cpp
    src/Engine/Files/filemanager.cpp
    src/Engine/Files/fileparser.cpp
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
    src/Engine/UI/progressbar.cpp
    src/Engine/UI/scrollable.cpp
    src/Engine/UI/sentencesystem.cpp
    src/Engine/UI/sizer.cpp
    src/Engine/UI/textinput.cpp
    src/Engine/UI/ttftext.cpp
    src/Engine/UI/uianimation.cpp
    src/Engine/UI/uiconstant.cpp
    src/Engine/UI/uisystem.cpp
)

# Create the static library
add_library(PgEngine STATIC ${ENGINESOURCE})

# Set precompiled headers
target_precompile_headers(PgEngine PUBLIC src/Engine/stdafx.h)

# Include directories - ONLY use generator expressions for proper build/install interface
target_include_directories(PgEngine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/Engine>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/GameElements>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${GLM_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${TASKFLOW_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${GLTF_DIR}/include>
    $<INSTALL_INTERFACE:include/PgEngine>
    $<INSTALL_INTERFACE:include/PgEngine/GameElements>
    $<INSTALL_INTERFACE:include>
)

# Add platform-specific build interface includes
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    target_include_directories(PgEngine PUBLIC
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
    target_link_libraries(PgEngine PUBLIC glm)
else()
    # Native platforms - link publicly so they get exported
    if (TARGET SDL2::SDL2main)
        target_link_libraries(PgEngine PUBLIC SDL2::SDL2main)
    endif()

    target_link_options(PgEngine PRIVATE -static-libgcc -static-libstdc++)
    
    # All dependencies as PUBLIC since we're exporting them
    # Check for target name variations and use the correct ones
    set(SDL2_TARGETS)
    if(TARGET SDL2::SDL2-static)
        list(APPEND SDL2_TARGETS SDL2::SDL2-static)
    elseif(TARGET SDL2-static)
        list(APPEND SDL2_TARGETS SDL2-static)
    endif()
    
    if(TARGET SDL2_mixer::SDL2_mixer-static)
        list(APPEND SDL2_TARGETS SDL2_mixer::SDL2_mixer-static)
    elseif(TARGET SDL2_mixer-static)
        list(APPEND SDL2_TARGETS SDL2_mixer-static)
    endif()
    
    if(TARGET SDL2_net::SDL2_net-static)
        list(APPEND SDL2_TARGETS SDL2_net::SDL2_net-static)
    elseif(TARGET SDL2_net-static)
        list(APPEND SDL2_TARGETS SDL2_net-static)
    elseif(TARGET SDL2_net)
        list(APPEND SDL2_TARGETS SDL2_net)
    endif()
    
    target_link_libraries(PgEngine PUBLIC 
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

# Check specific targets
foreach(target IN ITEMS SDL2main SDL2-static SDL2_net SDL2_mixer-static glm libglew_static freetype)
    if(TARGET ${target})
        message(STATUS "Target ${target} exists")
    else()
        message(STATUS "Target ${target} does NOT exist")
    endif()
endforeach()

# Installation - Install PgEngine and all its static dependencies
if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    # Emscripten build - only install PgEngine and glm
    install(TARGETS PgEngine glm
        EXPORT PgEngineTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
    )
else()
    # Native build - install all static dependencies
    # First check which targets actually exist
    set(TARGETS_TO_INSTALL PgEngine)
    
    # Add targets that exist (check for both alias and direct names)
    if(TARGET SDL2-static)
        list(APPEND TARGETS_TO_INSTALL SDL2-static)
    endif()
    
    if(TARGET SDL2main)
        list(APPEND TARGETS_TO_INSTALL SDL2main)
    endif()
    
    if(TARGET SDL2_mixer-static)
        list(APPEND TARGETS_TO_INSTALL SDL2_mixer-static)
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
        EXPORT PgEngineTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
    )
endif()

# Install ALL headers including dependencies
install(DIRECTORY src/Engine/
    DESTINATION include/PgEngine
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

install(DIRECTORY src/GameElements/
    DESTINATION include/PgEngine/GameElements
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Export targets - this creates the PgEngineTargets.cmake file
install(EXPORT PgEngineTargets
    FILE PgEngineTargets.cmake
    NAMESPACE PgEngine::
    DESTINATION lib/cmake/PgEngine
)

# Install vendored dependencies headers
install(DIRECTORY ${GLM_DIR}/glm/
    DESTINATION include/glm
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
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
    
    # Install SDL2_mixer headers
    install(DIRECTORY ${SDL2MIXER_DIR}/include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
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
    "${CMAKE_CURRENT_BINARY_DIR}/PgEngineConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PgEngineConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/PgEngineConfig.cmake"
    INSTALL_DESTINATION lib/cmake/PgEngine
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/PgEngineConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/PgEngineConfigVersion.cmake"
    DESTINATION lib/cmake/PgEngine
)

# Only build examples if not building static lib only
if(NOT BUILD_STATIC_LIB AND BUILD_EXAMPLES)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
        add_executable(GMTK2025 exemples/GMTK2025/main.cpp)

        target_include_directories(GMTK2025 PUBLIC exemples/GMTK2025)

        target_sources(GMTK2025 PRIVATE
            exemples/GMTK2025/application.cpp
            exemples/GMTK2025/ribbonmesh.cpp
            exemples/GMTK2025/polygonmesh.cpp
        )

        target_link_libraries(GMTK2025 PRIVATE PgEngine)

        set_target_properties(GMTK2025
            PROPERTIES SUFFIX ".html"
            LINK_FLAGS "-lidbfs.js -s FORCE_FILESYSTEM=1 -sPTHREAD_POOL_SIZE=4 -pthread -s ALLOW_MEMORY_GROWTH=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s SAFE_HEAP=1 ${USE_FLAGS}")

    else()
        add_executable(PgEngineEditor src/main.cpp)

        target_include_directories(PgEngineEditor PUBLIC src/Editor)

        target_sources(PgEngineEditor PRIVATE
            src/application.cpp
            src/application.h
            src/Editor/Gui/contextmenu.cpp
            src/Editor/Gui/inspector.cpp
            src/Editor/Gui/projectmanager.cpp
        )

        target_link_libraries(PgEngineEditor PRIVATE PgEngine)

        add_executable(GameOff exemples/GameOff/main.cpp)

        configure_file(res/nexus.pg nexus.pg COPYONLY)

        target_include_directories(GameOff PUBLIC exemples/GameOff)

        target_sources(GameOff PRIVATE
            exemples/GameOff/achievement.cpp
            exemples/GameOff/application.cpp
            exemples/GameOff/character.cpp
            exemples/GameOff/characustomizationscene.cpp
            exemples/GameOff/fightscene.cpp
            exemples/GameOff/gamefacts.cpp
            exemples/GameOff/inventory.cpp
            exemples/GameOff/item.cpp
            exemples/GameOff/location.cpp
            exemples/GameOff/locationscene.cpp
            exemples/GameOff/mainscene.cpp
            exemples/GameOff/managenerator.cpp
            exemples/GameOff/nexusscene.cpp
            exemples/GameOff/skilltree.cpp
            exemples/GameOff/passives.cpp
            exemples/GameOff/spells.cpp
            exemples/GameOff/theme.cpp
        )

        target_link_libraries(GameOff PRIVATE PgEngine)

        add_executable(PixelJam exemples/PixelJam/main.cpp)

        target_include_directories(PixelJam PUBLIC exemples/PixelJam)

        target_sources(PixelJam PRIVATE
            exemples/PixelJam/application.cpp
            exemples/PixelJam/Characters/enemy.cpp
            exemples/PixelJam/Characters/player.cpp
            exemples/PixelJam/config.cpp
            exemples/PixelJam/Aseprite_Lib/AsepriteLoader.cpp
            exemples/PixelJam/Aseprite_Lib/AsepriteFileAtlasLoader.cpp
            exemples/PixelJam/Tiled_Lib/TiledLoader.cpp
            exemples/PixelJam/Tiled_Lib/TileMapAtlasLoader.cpp
            exemples/PixelJam/Room/room.cpp
        )

        target_link_libraries(PixelJam PRIVATE PgEngine)

        add_executable(TugOfWar exemples/TugOfWar/main.cpp)

        target_include_directories(TugOfWar PUBLIC exemples/TugOfWar)

        target_sources(TugOfWar PRIVATE
            exemples/TugOfWar/application.cpp
        )

        target_link_libraries(TugOfWar PRIVATE PgEngine)

        ###

        add_executable(SimpleClientServer exemples/SimpleClientServer/main.cpp)

        target_sources(SimpleClientServer PRIVATE
            exemples/SimpleClientServer/application.cpp
        )

        target_link_libraries(SimpleClientServer PRIVATE PgEngine)

        ###

        add_executable(BasicTerminal exemples/BasicTerminal/main.cpp)

        target_include_directories(BasicTerminal PUBLIC exemples/BasicTerminal)

        target_sources(BasicTerminal PRIVATE
            exemples/BasicTerminal/application.cpp
        )

        target_link_libraries(BasicTerminal PRIVATE PgEngine)

        ###

        add_executable(EmptyApp exemples/EmptyAppTemplate/main.cpp)

        target_include_directories(EmptyApp PUBLIC exemples/EmptyAppTemplate)

        target_sources(EmptyApp PRIVATE
            exemples/EmptyAppTemplate/application.cpp
        )

        target_link_libraries(EmptyApp PRIVATE PgEngine)

        ###

        add_executable(GMTK2025 exemples/GMTK2025/main.cpp)

        target_include_directories(GMTK2025 PUBLIC exemples/GMTK2025)

        target_sources(GMTK2025 PRIVATE
            exemples/GMTK2025/application.cpp
            exemples/GMTK2025/ribbonmesh.cpp
            exemples/GMTK2025/polygonmesh.cpp
        )

        target_link_libraries(GMTK2025 PRIVATE PgEngine)

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

        target_link_libraries(t1 PRIVATE gtest gtest_main PgEngine)

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
        )

        target_link_libraries(bench PRIVATE gtest gtest_main PgEngine)
    endif()
endif()

# CPack configuration for creating installers
set(CPACK_PACKAGE_NAME "PgEngine")
set(CPACK_PACKAGE_VENDOR "PigeonCodeur")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "PgEngine - A modern C++ game engine")
set(CPACK_PACKAGE_DESCRIPTION "PgEngine is a feature-rich game engine with 2D/3D rendering, ECS architecture, audio system, and scripting capabilities.")
set(CPACK_PACKAGE_CONTACT "pigeoncodeur@gmail.com")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Gallasko/PgEngine")

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