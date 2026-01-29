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

    std::string fileName;
    bool enableProfiling = false;

    // Parse command line arguments
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];

        if (arg == "--profile" || arg == "-p")
        {
            enableProfiling = true;
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "Usage: " << argv[0] << " [options] <script.pg>" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --profile, -p    Enable profiling" << std::endl;
            std::cout << "  --help, -h       Show this help message" << std::endl;
            return 0;
        }
        else if (fileName.empty())
        {
            fileName = arg;
        }
    }

    CompilerApp app(fileName, enableProfiling, argc, argv);

    return app.exec();
}


