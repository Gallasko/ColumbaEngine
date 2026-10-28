#pragma once

#include <atomic>
#include <cstdarg>
#include <mutex>
#include <memory>

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

#include <ECS/entityref.h>

#include <Profiler/framelimiter.h>

namespace pg
{
    // Forward declarations
    class EntitySystem;
    class Input;
    class WslCursor;
    // class UiComponent;
    struct VM;
    class MasterRenderer;
    struct AudioSystem;
    using _unique_id = uint64_t;
    template<typename T> struct CompRef;
    struct Point2D;

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

        inline void receivedQuitRequest() { needToQuit = true; }

        inline bool requestQuit() const { return needToQuit; }

        const Input * getInputHandler() const { return inputHandler; }

        // Capture the mouse for a look camera; false gives the system cursor back.
        // Natively the cursor is hidden and warped to the window center (the
        // caller keeps it off the edges, see needsCursorRecentering). In a
        // browser it is the Pointer Lock API, granted on the next click or key
        // press. On WSLg a helper on the Windows side holds the real cursor
        // (see WslCursor), its motion arrives as OnSDLMouseMotion.
        void setCursorLocked(bool locked);

        // True while the mouse is really held, so OnSDLMouseMotion's xrel/yrel
        // can turn a camera. False while a lock is pending (browser waiting for
        // a click, WSL window without focus): the motion then is the free cursor.
        bool isCursorCaptured() const { return cursorCaptured.load(); }

        // True once after the platform let go of a lock that was still wanted
        // (Esc in a browser always ends Pointer Lock)
        bool takeCursorLockLost() { return cursorLockLost.exchange(false); }

        // Whether a look camera must keep the hidden cursor off the window edges
        // with SDL_WarpMouseInWindow: false where the capture has no edges
        // (browser, WSL helper) or the warp does nothing (WSL without helper)
        bool needsCursorRecentering() const;

        // Current window size in pixels. Exposed so input/look controllers
        // can do edge-based cursor recentering without re-querying SDL.
        int getWidth()  const { return width; }
        int getHeight() const { return height; }

        // Toggle between windowed and borderless-fullscreen (desktop
        // resolution). Bound to F10 in processEvents.
        void toggleFullscreen();

    public:
        EntitySystem* ecs;

        /** Script vm of the window, runs the boot scripts (res/logManager.pg, res/setupRenderer.pg, res/sysRegister.pg) */
        VM* vm = nullptr;

        MasterRenderer *masterRenderer = nullptr;

        /** Paces the render loop when a target FPS is set (0 = uncapped).
         *  Wired to EngineConfig::targetFPS; runtime-adjustable. */
        FrameLimiter renderFrameLimiter;

    protected:
        void swapBuffer();

    private:
        // Per frame: follows the platform's real lock state and feeds the WSL helper
        void updateCursorCapture();

        bool cursorLockWanted = false;
        bool pointerLockSeen = false;
        std::atomic<bool> cursorCaptured {false};
        std::atomic<bool> cursorLockLost {false};
        std::unique_ptr<WslCursor> wslCursor;
        bool wslCursorTried = false;

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
        // CompRef<UiComponent>* screenUi;

        Point2D* mousePos;

        std::mutex renderMutex;

        float xSensitivity = 1.0f;
        float ySensitivity = 1.0f;

        // Todo: remove this
        void* terminalSink; // std::shared_ptr<Logger::LogSink>
    };
}