cmake_minimum_required(VERSION 3.18)
#cmake_minimum_required(VERSION 3.13)

project(PgEngine VERSION 1.0)

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
set(SDL2_ttf_DIR "import/SDL2_ttf-2.22.0-mingw/cmake")
set(GLM_DIR "import/glm")
set(TASKFLOW_DIR "import/taskflow-3.6.0")
set(GTEST_DIR "import/googletest-1.14.0")
set(GLEW_DIR "import/glew-cmake")

find_package(OpenGL REQUIRED)

if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(USE_FLAGS "-sUSE_SDL=2 -sUSE_SDL_MIXER=2 -sUSE_SDL_TTF=2 -sUSE_FREETYPE=1 -fwasm-exceptions -sUSE_PTHREADS=1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${USE_FLAGS} --use-preload-plugins \
        --preload-file res \
        --preload-file scripts \
        --preload-file shader \
        --preload-file setupRenderer.pg \
        --preload-file logManager.pg")
    set(CMAKE_EXECUTABLE_SUFFIX .html)
else()
    add_subdirectory(${SDL2_DIR})
    #find_package(SDL2 REQUIRED)
    # add_subdirectory(${SDL2TTF_DIR})
    add_subdirectory(${SDL2MIXER_DIR})
endif()

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    add_subdirectory(${GLEW_DIR})
endif()

set(LINKER_FLAGS "")

# Todo make this work
# add_subdirectory(${SDL2TTF_DIR})

add_subdirectory(${GLM_DIR})
add_subdirectory(${TASKFLOW_DIR})

include_directories(${GLM_DIR}/glm)
include_directories(${TASKFLOW_DIR}/taskflow)

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    include_directories(${SDL2_DIR}/include)
    include_directories(import/SDL2_ttf-2.22.0-mingw/x86_64-w64-mingw32/include/SDL2)
    include_directories(${SDL2MIXER_DIR}/include)
    include_directories(${GLEW_DIR}/include)
endif()

if(UNIX AND NOT APPLE)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl2)
    find_package(SDL2_ttf REQUIRED)
    set(TTF_LIBS SDL2_ttf::SDL2_ttf)
else()
    # add_subdirectory(${SDL2_ttf_DIR})
    find_package(SDL2_ttf CONFIG REQUIRED)
    set(TTF_LIBS SDL2_ttf::SDL2_ttf)
endif()

# Debug Flags
if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(CMAKE_C_FLAGS_DEBUG "-Wall -Wextra -g -pthread -ggdb -O0 -DDEBUG ${USE_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "-Wall -Wextra -g -pthread -ggdb -O0 -DDEBUG ${USE_FLAGS}")
else()
# todo add a 32bit debug "-m32"
    set(CMAKE_CXX_FLAGS_DEBUG "-Wall -Wextra -g -pthread -Wa,-mbig-obj -O0 -ggdb -DDEBUG")
endif()

# set(CMAKE_CXX_FLAGS_DEBUG "-std=c++17 -Wall -Wextra -g -pthread -Wa,-mbig-obj -s -O3 -DNDEBUG -mwindows")

# Release Flags
if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
# set(CMAKE_CXX_FLAGS_RELEASE "-Wall -Wextra -pthread -Wa,-mbig-obj -s -O3 -DNDEBUG -mwindows")
    # Use -g -O0 for fast debugging !
    set(CMAKE_C_FLAGS_RELEASE "-Wall -Wextra -pthread -O3 -DNDEBUG -m32 ${USE_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "-Wall -Wextra -pthread -O3 -DNDEBUG -m32 ${USE_FLAGS}")
    # set(CMAKE_C_FLAGS_RELEASE "-Wall -Wextra -pthread -g -O0 -DNDEBUG -m32 ${USE_FLAGS}")
    # set(CMAKE_CXX_FLAGS_RELEASE "-Wall -Wextra -pthread -g -O0 -DNDEBUG -m32 ${USE_FLAGS}")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-Wall -Wextra -pthread -s -O3 -DNDEBUG")
endif()

message("Current c++ debug flags used: '${CMAKE_CXX_FLAGS_DEBUG}'")
message("Current c++ release flags used: '${CMAKE_CXX_FLAGS_RELEASE}'")

set(ENGINESOURCE
    src/Engine/window.cpp
    src/Engine/configuration.cpp
    src/Engine/logger.cpp
    src/Engine/serialization.cpp
    src/Engine/2D/collisionsystem.cpp
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
    src/Engine/Systems/uimodule.cpp
    src/Engine/UI/button.cpp
    src/Engine/UI/focusable.cpp
    src/Engine/UI/listview.cpp
    src/Engine/UI/scrollable.cpp
    src/Engine/UI/sentencesystem.cpp
    src/Engine/UI/textinput.cpp
    src/Engine/UI/ttftext.cpp
    src/Engine/UI/uianimation.cpp
    src/Engine/UI/uiconstant.cpp
    src/Engine/UI/uisystem.cpp
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    # add_executable(TetrisClone src/Tetris/tetris.cpp)

    # target_sources(TetrisClone PRIVATE
    #     src/Tetris/app.cpp
    #     src/Tetris/tetromino.cpp
    #     src/Tetris/keyconfig.cpp
    #     src/Tetris/titlescreen.cpp
    #     ${ENGINESOURCE}
    # )
    add_executable(Gmtk src/GMTK/gmtk.cpp)

    target_sources(Gmtk PRIVATE
        src/GMTK/app.cpp
        src/GMTK/game.cpp
        src/GMTK/titlescreen.cpp
        ${ENGINESOURCE}
        )

    target_include_directories(Gmtk PRIVATE src/GMTK)
    target_include_directories(Gmtk PUBLIC src/Engine)
    target_include_directories(Gmtk PUBLIC src/GameElements)

    set_target_properties(Gmtk
        PROPERTIES SUFFIX ".html"
        # LINK_FLAGS " -s USE_FREETYPE=1 -sGL_ENABLE_GET_PROC_ADDRESS -s DEMANGLE_SUPPORT=1 -s SAFE_HEAP=1 --bind -s WASM=1 -O3 -s LEGACY_GL_EMULATION=0 -s GL_UNSAFE_OPTS=0 -s ASSERTIONS=1 -s GL_ASSERTIONS=1 -s INVOKE_RUN=0 -std=c++17 -s USE_WEBGL2=1 -s FULL_ES3=1 -s USE_GLFW=3 -s OFFSCREENCANVAS_SUPPORT=1")
        # LINK_FLAGS " -s USE_FREETYPE=1 -sGL_ENABLE_GET_PROC_ADDRESS -s DEMANGLE_SUPPORT=1 -s SAFE_HEAP=1 --bind -s WASM=1 -O3 -s LEGACY_GL_EMULATION=0 -s GL_UNSAFE_OPTS=0 -s ASSERTIONS=1 -s GL_ASSERTIONS=1 -s INVOKE_RUN=0 -std=c++17 -s USE_WEBGL2=1 -s FULL_ES3=1 -s USE_GLFW=3 -s OFFSCREENCANVAS_SUPPORT=1")
        # LINK_FLAGS " -s USE_FREETYPE=1 -sGL_ENABLE_GET_PROC_ADDRESS -s DISABLE_EXCEPTION_CATCHING=0 -s DEMANGLE_SUPPORT=1 -s SAFE_HEAP=1 --bind -s WASM=1 -O3 -s LEGACY_GL_EMULATION=0 -s GL_UNSAFE_OPTS=0 -s ASSERTIONS=1 -s GL_ASSERTIONS=1 -s INVOKE_RUN=0 -std=c++17 -s USE_WEBGL2=1 -s FULL_ES3=1 -s USE_GLFW=3 -s OFFSCREENCANVAS_SUPPORT=1")

        #--bind -m32 -s USE_FREETYPE=1 -s USE_WEBGL2=1 -s WASM=0 -s MIN_WEBGL_VERSION=1 -s ABORT_ON_WASM_EXCEPTIONS=1 -g2 -s USE_GLFW=3 -sSAFE_HEAP")

        LINK_FLAGS "-lidbfs.js -s FORCE_FILESYSTEM=1 -sPTHREAD_POOL_SIZE=4 -pthread -s ALLOW_MEMORY_GROWTH=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s SAFE_HEAP=1 -s ASSERTIONS=1 -O3 ${USE_FLAGS}")
        #  -sPTHREAD_POOL_SIZE=30 -s PROXY_TO_PTHREAD=1 -s ALLOW_MEMORY_GROWTH=1 -s USE_GLFW=3 -s USE_WEBGL2=1 -s GL_ASSERTIONS=1 -s WASM=1
        # LINK_FLAGS " -s USE_FREETYPE=1 -sGL_ENABLE_GET_PROC_ADDRESS -s DISABLE_EXCEPTION_CATCHING=0 -s DEMANGLE_SUPPORT=1 -s SAFE_HEAP=1 --bind -s WASM=1 -O3 -s LEGACY_GL_EMULATION=0 -s GL_UNSAFE_OPTS=0 -s ASSERTIONS=1 -s GL_ASSERTIONS=1 -s INVOKE_RUN=0 -std=c++17 -s USE_WEBGL2=1 -s FULL_ES3=1 -s USE_GLFW=3 -s OFFSCREENCANVAS_SUPPORT=1")

        #--bind -m32 -s USE_FREETYPE=1 -s USE_WEBGL2=1 -s WASM=0 -s MIN_WEBGL_VERSION=1 -s ABORT_ON_WASM_EXCEPTIONS=1 -g2 -s USE_GLFW=3 -sSAFE_HEAP")
    

    target_link_libraries(Gmtk PRIVATE glm)

else()
    add_library(PgEngineSrc ${ENGINESOURCE})

    target_include_directories(PgEngineSrc PUBLIC src/Engine)
    target_include_directories(PgEngineSrc PUBLIC src/GameElements)

    if(TARGET SDL2::SDL2main)
        target_link_libraries(PgEngineSrc PRIVATE SDL2::SDL2main)
    endif()

    # target_link_libraries(PgEngine PRIVATE SDL2::SDL2-static SDL2_ttf::SDL2_ttf-static SDL2_mixer::SDL2_mixer-static glm)
    target_link_options(PgEngineSrc PRIVATE -static-libgcc -static-libstdc++)

    # target_link_libraries(PgEngineSrc PRIVATE SDL2::SDL2-static SDL2_ttf::SDL2_ttf-static SDL2_mixer::SDL2_mixer-static glm libglew_static)
    if(UNIX AND NOT APPLE)
        target_link_libraries(PgEngineSrc PRIVATE SDL2::SDL2-static SDL2::TTF SDL2_mixer::SDL2_mixer-static glm libglew_static)
    else()
        target_link_libraries(PgEngineSrc PRIVATE SDL2::SDL2-static SDL2_ttf::SDL2_ttf-static SDL2_mixer::SDL2_mixer-static glm libglew_static)
    endif()
    
    # target_link_libraries(PgEngineSrc PRIVATE $<IF:$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf>,SDL2_ttf::SDL2_ttf,SDL2_ttf::SDL2_ttf-static>)

    # find_package(PgEngineSrc REQUIRED)

    add_executable(PgEngine src/main.cpp)

    target_include_directories(PgEngineSrc PUBLIC src/Editor)

    add_executable(TetrisClone exemples/TetrisClone/tetris.cpp)

    target_sources(PgEngine PRIVATE
        src/application.cpp
        src/application.h
        src/Editor/Gui/contextmenu.cpp
        src/Editor/Gui/inspector.cpp
    )

    target_link_libraries(PgEngine PRIVATE PgEngineSrc)

    target_sources(TetrisClone PRIVATE
        exemples/TetrisClone/app.cpp
        exemples/TetrisClone/tetromino.cpp
        exemples/TetrisClone/keyconfig.cpp
        exemples/TetrisClone/titlescreen.cpp
        )

    target_include_directories(TetrisClone PRIVATE exemples/TetrisClone)

    target_link_libraries(TetrisClone PRIVATE PgEngineSrc)

    # enable_testing()

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
        # test/mock2dsimpleshape.h
        # test/simple2dobject.cc
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

endif()

# add_custom_command(TARGET t1 POST_BUILD 
#   COMMAND "${CMAKE_COMMAND}" -E copy 
#      "../testdeps"
#      "." 
#   COMMENT "Copying test dependencies")

# target_include_directories(t1 PRIVATE test)

#gtest_discover_tests(t1)