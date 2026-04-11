#include "application.h"

#include "ECS/entitysystem.h"
#include "Renderer/renderer.h"
#include "Input/input.h"
#include "window.h"
#include "logger.h"

#include "voxelcomponents.h"
#include "voxelrenderer.h"
#include "camera3dcontroller.h"

#include "glm/glm.hpp"

using namespace pg;

namespace
{
    static const char *const DOM = "App";
}

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        auto* masterRenderer = window.masterRenderer;
        const auto* input    = window.getInputHandler();

        const auto& cfg = engine.getConfig();
        const float aspect = (cfg.height > 0)
            ? static_cast<float>(cfg.width) / static_cast<float>(cfg.height)
            : 16.0f / 9.0f;

        // --- Systems ---
        ecs.createSystem<Camera3DController>(masterRenderer, input);
        ecs.createSystem<VoxelRenderSystem>(masterRenderer, aspect);

        // MasterRenderer consumes render calls — make sure it runs AFTER
        // our producers so the view matrix and voxel render calls are
        // up-to-date each frame. `succeed<A, B>` means A runs after B.
        ecs.succeed<MasterRenderer, Camera3DController>();
        ecs.succeed<MasterRenderer, VoxelRenderSystem>();

        // --- Test scene: 32x32 checkerboard ground plane ---
        for (int x = -16; x < 16; ++x)
        {
            for (int z = -16; z < 16; ++z)
            {
                const bool even = ((x + z) & 1) == 0;

                const glm::vec4 col = even
                    ? glm::vec4(180.0f, 180.0f, 180.0f, 255.0f)
                    : glm::vec4(100.0f, 100.0f, 100.0f, 255.0f);

                auto ent = ecs.createEntity();
                ecs.attach<VoxelComponent>(ent,
                    glm::vec3(static_cast<float>(x), -1.0f, static_cast<float>(z)),
                    glm::vec3(1.0f, 0.1f, 1.0f),
                    col);
            }
        }

        // --- Test scene: 5 landmark voxels scattered around the origin ---
        struct Marker { glm::vec3 pos; glm::vec3 size; glm::vec4 color; };

        const Marker markers[] =
        {
            { {  0.0f, 0.0f,  0.0f }, { 1.0f, 1.0f, 1.0f }, { 255.0f,   0.0f,   0.0f, 255.0f } }, // red — origin
            { {  5.0f, 0.0f,  0.0f }, { 1.0f, 2.0f, 1.0f }, {   0.0f, 255.0f,   0.0f, 255.0f } }, // green — +X tall
            { {  0.0f, 0.0f,  5.0f }, { 1.0f, 1.0f, 1.0f }, {   0.0f,   0.0f, 255.0f, 255.0f } }, // blue — +Z
            { {  3.0f, 0.0f,  3.0f }, { 2.0f, 1.0f, 2.0f }, { 255.0f, 255.0f,   0.0f, 255.0f } }, // yellow — flat slab
            { { -4.0f, 0.0f, -2.0f }, { 1.0f, 3.0f, 1.0f }, { 255.0f,   0.0f, 255.0f, 255.0f } }, // magenta — tower
        };

        for (const auto& m : markers)
        {
            auto ent = ecs.createEntity();
            ecs.attach<VoxelComponent>(ent, m.pos, m.size, m.color);
        }

        LOG_INFO(DOM, "VoxelTest scene ready: ground plane + 5 markers");
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
