#include <iostream>

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "application.h"

#include "logger.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <SDL2/SDL.h>
#include <SDL_opengles2.h>
// #include <SDL_opengl_glext.h>
#include <GLES2/gl2.h>
// #include <GLFW/glfw3.h>
#else
#ifdef __linux__
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#elif _WIN32
#include <SDL.h>
#include <SDL_opengl.h>
#endif
#include <GL/gl.h>
#endif

//[TODO] Variant using operator* dereferencing to recast to the original type
int main(int argc, char *argv[])
{
    // Decouple C++ and C stream for faster runtime
    std::ios_base::sync_with_stdio(false);
    // auto fileSink = pg::Logger::registerSink<pg::FileSink>();
#ifdef __EMSCRIPTEN__
    printf("Starting program...\n");
#endif
    GameApp app("Pixel Jam");

    return app.exec();
}

