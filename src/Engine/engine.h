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

#include "Versioning/versionmanager.h"

namespace pg
{
    // Forward declarations
    class Window;
    class EntitySystem;

    struct EngineConfig
    {
        int width = 820;
        int height = 640;
        bool resizable = true;
        bool fullscreen = false;
        std::string saveFolder = "save";
        std::string saveSystemFile = "system.sz";
        std::string manifestPath = "manifest.json";
        bool autoWipeSaveOnMajorBump = true;
        bool autoRunMigrations = true;
        bool vsync = true;

#ifdef PROFILE
        /** Profiling builds default to a readable capture: 30 FPS render,
         *  5 ECS passes phase-locked to each rendered frame. Runtime
         *  adjustable from the profiler overlay. */
        int targetFPS = 30;
        int ecsPassesPerFrame = 5;
#else
        int targetFPS = 60;

        /** Phase-lock the ECS loop to N passes per rendered frame, spread
         *  evenly across the frame (0 = disabled). */
        int ecsPassesPerFrame = 0;
#endif

        /** Free-running FPS cap for the ECS graph loop (0 = uncapped).
         *  Only used when ecsPassesPerFrame is 0. */
        int ecsTargetFPS = 0;
        bool autoStartECS = true;  // If false, ECS must be started manually via getECS()->start()
    };

    class Engine
    {
    public:
        Engine(const std::string& appName, const EngineConfig& config = {});
        ~Engine();

        void setConfig(const EngineConfig& config) { this->config = config; savePath = constructSavePath(); }

        Engine& setSetupFunction(std::function<void(EntitySystem&, Window&)> setup);
        Engine& setPostInitFunction(std::function<void(EntitySystem&, Window&)> postInit);

        int exec();

        Window* getWindow() const { return mainWindow; }
        EntitySystem* getECS() const;
        const EngineConfig& getConfig() const { return config; }
        const std::string& getAppName() const { return appName; }

        VersionManager& getVersionManager() { return versionManager; }
        const VersionManager& getVersionManager() const { return versionManager; }

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
        VersionManager versionManager;

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