#pragma once

#ifdef __EMSCRIPTEN__
    #include <SDL2/SDL.h>
#else
    #ifdef __linux__
        #include <SDL2/SDL.h>
    #elif _WIN32
        #include <SDL.h>
    #endif
#endif

#include <functional>
#include <memory>
#include <string>
#include <atomic>
#include <thread>

namespace pg
{
    // Forward declarations
    class Window;
    class EntitySystem;

    struct EngineConfig
    {
        int width = 800;
        int height = 600;
        bool resizable = true;
        bool fullscreen = false;
        std::string saveFolder = "save";
        std::string saveSystemFile = "system.sz";
        bool vsync = true;
        int targetFPS = 60;
    };

    class Engine
    {
    public:
        Engine(const std::string& appName, const EngineConfig& config = {});
        ~Engine();

        Engine& setSetupFunction(std::function<void(EntitySystem&, Window&)> setup);
        Engine& setPostInitFunction(std::function<void(EntitySystem&, Window&)> postInit);

        int exec();

        Window* getWindow() const { return mainWindow; }
        EntitySystem* getECS() const;
        const EngineConfig& getConfig() const { return config; }
        const std::string& getAppName() const { return appName; }

        bool isWindowReady() const { return windowReady.load(); }
        bool isECSReady() const { return ecsReady.load(); }
        bool isFullyInitialized() const { return initialized; }

        // Public for callback access
        std::string appName;
        EngineConfig config;
        std::function<void(EntitySystem&, Window&)> setup = nullptr;
        std::function<void(EntitySystem&, Window&)> postInit = nullptr;
        Window* mainWindow = nullptr;
        std::atomic<bool> windowReady{false};
        std::atomic<bool> ecsReady{false};
        bool initialized = false;
        std::string savePath;

#ifdef __EMSCRIPTEN__
        std::thread* initThread = nullptr;

    public:
        void initializeECS();
#else
    private:
        void initializeECS();
#endif

    private:
        void initializeWindow();
        void setupFilesystem();
        std::string constructSavePath() const;
    };

} // namespace pg