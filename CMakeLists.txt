cmake_minimum_required(VERSION 3.18)
# cmake_minimum_required(VERSION 3.13)

project(PgEngine VERSION 1.0)

option(UseGDB "UseGDB" OFF)
option(ENABLE_TIME_TRACE "Add -ftime-trace to Clang builds (for compile-time profiling)" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

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

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    add_compile_options(-DGLEW_STATIC)
endif()

set(SDL2_DIR "import/SDL2-2.28.5")
set(SDL2MIXER_DIR "import/SDL2_mixer-2.6.3")
set(SDL2NET_DIR "import/SDL2_net-2.2.0")
set(GLM_DIR "import/glm")
set(TASKFLOW_DIR "import/taskflow-3.6.0")
set(GTEST_DIR "import/googletest-1.14.0")
set(GLEW_DIR "import/glew-cmake")
set(TTF_DIR "import/freetype-2.13.3")
set(GLTF_DIR "import/tinygltf")

find_package(OpenGL REQUIRED)

if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
# set(USE_FLAGS "-sUSE_SDL=2 -sUSE_SDL_MIXER=2 -sUSE_SDL_TTF=2 -sUSE_FREETYPE=1 -fwasm-exceptions -sUSE_PTHREADS=1")
set(USE_FLAGS "-sUSE_SDL=2 -sUSE_SDL_MIXER=2 -sUSE_SDL_NET=2 -sUSE_FREETYPE=1 -fwasm-exceptions -sUSE_PTHREADS=1")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${USE_FLAGS} --use-preload-plugins \
        --preload-file res \
        --preload-file scripts \
        --preload-file shader \
        --preload-file setupRenderer.pg \
        --preload-file sysRegister.pg \
        --preload-file logManager.pg")
    set(CMAKE_EXECUTABLE_SUFFIX .html)
else()
    add_subdirectory(${SDL2_DIR})
    add_subdirectory(${SDL2MIXER_DIR})
    add_subdirectory(${SDL2NET_DIR})
    add_subdirectory(${GLEW_DIR})
    add_subdirectory(${TTF_DIR})
    add_subdirectory(${GLTF_DIR})

    if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
        set(CMAKE_CXX_FLAGS "-Wa,-mbig-obj -Wall -Wextra -pthread -s -O3 -DNDEBUG")
    else()
        # Todo make sure that -03 is enabled when release
        if(${CMAKE_HOST_SYSTEM} MATCHES "Linux")
        # set(CMAKE_CXX_FLAGS "-flto -Wall -Wextra -pthread -O3 -DNDEBUG")
        # set(CMAKE_CXX_FLAGS "-ftime-trace -Wall -Wextra -pthread -O0 -ggdb -DDEBUG -DPROFILE")
        # -Wdouble-promotion

            # Only add -ftime-trace if compiler is Clang
            set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wnon-virtual-dtor -Woverloaded-virtual -Wfloat-equal -pedantic -pthread -O0 -ggdb -DDEBUG")

            if (${ENABLE_TIME_TRACE})
                if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftime-trace")
                endif()
            endif()
        endif()

        if(${CMAKE_HOST_SYSTEM} MATCHES "Windows")
            set(CMAKE_CXX_FLAGS " -Wall -Wextra -pthread -Wa,-mbig-obj -s -O2 -DNDEBUG")
        endif()

    endif()

    # set(CMAKE_CXX_FLAGS "-Wall -Wextra -g -pthread -Wa,-mbig-obj -O0 -ggdb -DDEBUG")
endif()

if(${UseGDB} EQUAL ON)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb")
endif()

add_subdirectory(${GLM_DIR})
# add_subdirectory(${TASKFLOW_DIR})

include_directories(${GLM_DIR}/glm)
include_directories(${TASKFLOW_DIR}/taskflow)
include_directories(${GLTF_DIR}/include)

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    include_directories(${SDL2_DIR}/include)
    include_directories(${SDL2MIXER_DIR}/include)
    include_directories(${SDL2NET_DIR}/include)
    include_directories(${GLEW_DIR}/include)
    include_directories(${TTF_DIR}/include)
endif()

# Debug Flags
if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(CMAKE_C_FLAGS_DEBUG "-Wall -Wextra -g -pthread -ggdb -O0 -DDEBUG ${USE_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "-Wall -Wextra -g -pthread -ggdb -O0 -DDEBUG ${USE_FLAGS}")
else()
# todo add a 32bit debug "-m32"
    set(CMAKE_CXX_FLAGS_DEBUG "-Wall -Wextra -g -pthread -Wa,-mbig-obj -O0 -ggdb -DDEBUG")
endif()

# Release Flags
if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    # Use -g -O0 for fast debugging !
    set(CMAKE_C_FLAGS_RELEASE "-Wall -Wextra -pthread -O3 -DNDEBUG -m32 ${USE_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "-Wall -Wextra -pthread -O3 -DNDEBUG -m32 ${USE_FLAGS}")
    # set(CMAKE_C_FLAGS_RELEASE "-Wall -Wextra -pthread -g -O0 -DNDEBUG -m32 ${USE_FLAGS}")
    # set(CMAKE_CXX_FLAGS_RELEASE "-Wall -Wextra -pthread -g -O0 -DNDEBUG -m32 ${USE_FLAGS}")
else()
    # Todo add -mwindows to disable the shell window opening
    # set(CMAKE_CXX_FLAGS_RELEASE "-ftime-trace -Wall -Wextra -pthread -O3 -DNDEBUG")
    # set(CMAKE_CXX_FLAGS_RELEASE "-Wall -Wextra -pthread -s -O3 -DNDEBUG")
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

add_library(PgEngineSrc ${ENGINESOURCE})

target_precompile_headers(PgEngineSrc PUBLIC
    src/Engine/stdafx.h
)

# target_precompile_headers(PgEngineSrc PUBLIC
#     src/Engine/stdafx.h)

# target_precompile_headers(PgEngineSrc PRIVATE
    # import/taskflow-3.6.0/taskflow/taskflow.hpp)

if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    # add_executable(TetrisClone src/Tetris/tetris.cpp)

    # target_sources(TetrisClone PRIVATE
    #     src/Tetris/app.cpp
    #     src/Tetris/tetromino.cpp
    #     src/Tetris/keyconfig.cpp
    #     src/Tetris/titlescreen.cpp
    #     ${ENGINESOURCE}
    # )

    target_include_directories(PgEngineSrc PUBLIC src/Engine)
    target_include_directories(PgEngineSrc PUBLIC src/GameElements)
    target_include_directories(PgEngineSrc PUBLIC src/Editor)

    add_executable(GameOff exemples/GameOff/main.cpp)

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
    )

    target_link_libraries(GameOff PRIVATE PgEngineSrc)

    set_target_properties(GameOff
        PROPERTIES SUFFIX ".html"
        LINK_FLAGS "-lidbfs.js -s FORCE_FILESYSTEM=1 -sPTHREAD_POOL_SIZE=4 -pthread -s ALLOW_MEMORY_GROWTH=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s SAFE_HEAP=1 -s ASSERTIONS=1 -O3 ${USE_FLAGS}")

    target_link_libraries(GameOff PRIVATE glm)

    # add_executable(PgEngine src/main.cpp)

    # target_sources(PgEngine PRIVATE
    #     src/application.cpp
    #     src/application.h
    #     src/Editor/Gui/contextmenu.cpp
    #     src/Editor/Gui/inspector.cpp
    # )

    # target_link_libraries(PgEngine PRIVATE PgEngineSrc)

    # set_target_properties(PgEngine
    #     PROPERTIES SUFFIX ".html"
    #     LINK_FLAGS "-lidbfs.js -s FORCE_FILESYSTEM=1 -sPTHREAD_POOL_SIZE=4 -pthread -s ALLOW_MEMORY_GROWTH=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s SAFE_HEAP=1 -s ASSERTIONS=1 -O3 ${USE_FLAGS}")

    # target_link_libraries(PgEngine PRIVATE glm)

else()
    target_include_directories(PgEngineSrc PUBLIC src/Engine)
    target_include_directories(PgEngineSrc PUBLIC src/GameElements)

    if (TARGET SDL2::SDL2main)
        target_link_libraries(PgEngineSrc PRIVATE SDL2::SDL2main)
    endif()

    # target_link_libraries(PgEngine PRIVATE SDL2::SDL2-static SDL2_ttf::SDL2_ttf-static SDL2_mixer::SDL2_mixer-static glm)
    target_link_options(PgEngineSrc PRIVATE -static-libgcc -static-libstdc++)

    # target_link_libraries(PgEngineSrc PRIVATE SDL2::SDL2-static SDL2_ttf::SDL2_ttf-static SDL2_mixer::SDL2_mixer-static glm libglew_static)
    target_link_libraries(PgEngineSrc PRIVATE SDL2::SDL2-static SDL2_mixer::SDL2_mixer-static glm libglew_static freetype)

    add_executable(PgEngine src/main.cpp)

    target_include_directories(PgEngine PUBLIC src/Editor)

    target_sources(PgEngine PRIVATE
        src/application.cpp
        src/application.h
        src/Editor/Gui/contextmenu.cpp
        src/Editor/Gui/inspector.cpp
        src/Editor/Gui/projectmanager.cpp
    )

    target_link_libraries(PgEngine PRIVATE PgEngineSrc)

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

    target_link_libraries(GameOff PRIVATE PgEngineSrc)

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

    target_link_libraries(PixelJam PRIVATE PgEngineSrc)

    add_executable(TugOfWar exemples/TugOfWar/main.cpp)

    target_include_directories(TugOfWar PUBLIC exemples/TugOfWar)

    target_sources(TugOfWar PRIVATE
        exemples/TugOfWar/application.cpp
    )

    target_link_libraries(TugOfWar PRIVATE PgEngineSrc)

    ###

    add_executable(SimpleClientServer exemples/SimpleClientServer/main.cpp)

    target_include_directories(SimpleClientServer PUBLIC exemples/SimpleClientServer)

    target_sources(SimpleClientServer PRIVATE
        exemples/SimpleClientServer/application.cpp
    )

    target_link_libraries(SimpleClientServer PRIVATE PgEngineSrc)

    ###

    add_executable(EmptyApp exemples/EmptyAppTemplate/main.cpp)

    target_include_directories(EmptyApp PUBLIC exemples/EmptyAppTemplate)

    target_sources(EmptyApp PRIVATE
        exemples/EmptyAppTemplate/application.cpp
    )

    target_link_libraries(EmptyApp PRIVATE PgEngineSrc)

    # add_executable(TetrisClone exemples/TetrisClone/tetris.cpp)

    # target_sources(TetrisClone PRIVATE
    #     exemples/TetrisClone/app.cpp
    #     exemples/TetrisClone/tetromino.cpp
    #     exemples/TetrisClone/keyconfig.cpp
    #     exemples/TetrisClone/titlescreen.cpp
    # )

    # target_include_directories(TetrisClone PRIVATE exemples/TetrisClone)

    # target_link_libraries(TetrisClone PRIVATE PgEngineSrc)

    enable_testing()

    # find_package(GTest REQUIRED)

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

    target_link_libraries(t1 PRIVATE gtest gtest_main PgEngineSrc)

    add_custom_command(TARGET t1 PRE_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/testdeps/ $<TARGET_FILE_DIR:t1>)

    # add_test(NAME unit_tests COMMAND t1)

    include(GoogleTest)

    gtest_discover_tests(t1
        PROPERTIES
        FAIL_REGULAR_EXPRESSION "FAILED"
    )

    # gtest_discover_tests(t1)

    add_executable(bench benchmark/mainbenchmark.cc)

    target_sources(bench PRIVATE
        benchmark/memorypool.cc
    )

    target_link_libraries(bench PRIVATE gtest gtest_main PgEngineSrc)

endif()

# add_custom_command(TARGET t1 POST_BUILD
#   COMMAND "${CMAKE_COMMAND}" -E copy
#      "../testdeps"
#      "."
#   COMMENT "Copying test dependencies")

# target_include_directories(t1 PRIVATE test)

#gtest_discover_tests(t1)