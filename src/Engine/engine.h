#pragma once

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

    // Base interface for app initialization callbacks
    class AppInitializer
    {
    public:
        virtual ~AppInitializer() = default;
        virtual void setupSystems(EntitySystem& ecs, Window& window) = 0;
        virtual void postInit(EntitySystem& ecs, Window& window) = 0;
    };

    // Simple function-based initializer
    class FunctionInitializer : public AppInitializer
    {
    private:
        std::function<void(EntitySystem&, Window&)> setupFunc;
        std::function<void(EntitySystem&, Window&)> postInitFunc;

    public:
        FunctionInitializer(
            std::function<void(EntitySystem&, Window&)> setup,
            std::function<void(EntitySystem&, Window&)> postInit = nullptr) :
            setupFunc(setup), postInitFunc(postInit)
        {

        }

        void setupSystems(EntitySystem& ecs, Window& window) override
        {
            if (setupFunc)
                setupFunc(ecs, window);
        }

        void postInit(EntitySystem& ecs, Window& window) override
        {
            if (postInitFunc)
                postInitFunc(ecs, window);
        }
    };

    class Engine
    {
    public:
        Engine(const std::string& appName, const EngineConfig& config = {});
        ~Engine();

        Engine& setInitializer(std::unique_ptr<AppInitializer> init);
        Engine& setSetupFunction(std::function<void(EntitySystem&, Window&)> setup,
            std::function<void(EntitySystem&, Window&)> postInit = nullptr);

        int exec();

        Window* getWindow() const { return mainWindow; }
        EntitySystem* getECS() const;
        const EngineConfig& getConfig() const { return config; }
        const std::string& getAppName() const { return appName; }

        bool isWindowReady() const { return windowReady.load(); }
        bool isECSReady() const { return ecsReady.load(); }
        bool isFullyInitialized() const { return initialized; }

    private:
        std::string appName;
        EngineConfig config;
        std::unique_ptr<AppInitializer> initializer;

        Window* mainWindow = nullptr;
        std::atomic<bool> windowReady{false};
        std::atomic<bool> ecsReady{false};
        bool initialized = false;
        std::string savePath;

        void initializeWindow();
        void initializeECS();
        void setupFilesystem();
        std::string constructSavePath() const;
        void windowInitCallback();
        void mainLoopCallback(void* arg);

    #ifdef __EMSCRIPTEN__
        std::thread* initThread = nullptr;
    #endif
    };

} // namespace pg