#pragma once

#include <memory>

#include "ECS/entitysystem.h"

#include "Input/inputcomponent.h"

// Todo see if we support multiple window rendering in which case we need to correctly send the events to correct window

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
#elif _WIN32
#include <SDL.h>
#endif
#include <GL/gl.h>
#endif

namespace pg
{
    // Type forwarding
    class PgInterpreter;
    class MasterRenderer;
    class Input;
    class UiComponent;
    struct AudioSystem;

    class Window
    {
    public:
        Window(const std::string &title, const std::string& savePath = "save/savedata.sz");
        virtual ~Window();

        bool init(int width, int height, bool isFullscreen, SDL_Window* sdlWindow = nullptr);
        virtual bool initEngine();

        virtual void processEvents(const SDL_Event& event);

        void resize(int width, int height);

        virtual void render();

        inline bool requestQuit() const { return needToQuit; }

        const Input * getInputHandler() const { return inputHandler; }

    public:
        EntitySystem ecs;

        PgInterpreter *interpreter = nullptr;

        MasterRenderer *masterRenderer = nullptr;

    protected:
        void swapBuffer();

    private:
        SDL_Window* window = nullptr;
        SDL_GLContext context;

        std::string title;
        bool needToQuit = false;

        uint64_t currentTime = 0;

        uint32_t nbFrame = 0;

        int width = 1, height = 1;

        Input *inputHandler = nullptr;
        AudioSystem *audioSystem = nullptr;

        EntityRef screenEntity;
        CompRef<UiComponent> screenUi;

        MousePos mousePos;

        std::mutex renderMutex;

        float xSensitivity = 1.0f;
        float ySensitivity = 1.0f;

        std::shared_ptr<Logger::LogSink> terminalSink;
    };
}