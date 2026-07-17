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
#include <GLES2/gl2.h>
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

int main(int argc, char *argv[])
{
    std::ios_base::sync_with_stdio(false);

    static GameApp app("Atlas Picker");

    return app.exec();
}
