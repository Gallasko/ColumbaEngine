#include "application.h"

#include "ECS/entitysystem.h"
#include "Renderer/renderer.h"
#include "Input/input.h"
#include "window.h"
#include "logger.h"

#include "voxelcomponents.h"
#include "voxelrenderer.h"
#include "camera3dcontrollereditor.h"
#include "editorsystem.h"
#include "editorui.h"

#include "2D/simple2dobject.h"
#include "UI/ttftext.h"

#include "glm/glm.hpp"

using namespace pg;

namespace
{
    static const char *const DOM = "App";

    // Canvas dimensions — change these to set the editor workspace size.
    constexpr int CANVAS_W = 16;
    constexpr int CANVAS_H = 16;
    constexpr int CANVAS_D = 16;
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

        // --- Canvas (owned by GameApp, lifetime matches engine) ---
        canvas = std::make_unique<Canvas>(CANVAS_W, CANVAS_H, CANVAS_D);

        // Add a default layer so the editor is immediately usable.
        canvas->layers.push_back(Layer{ "Layer 1", true, {} });

        // --- Systems ---
        auto* cam    = ecs.createSystem<Camera3DControllerEditor>(masterRenderer, input, &window);
        ecs.createSystem<VoxelRenderSystem>(masterRenderer, aspect);
        auto* editor = ecs.createSystem<EditorSystem>(masterRenderer, &window, cam, canvas.get());

        // Engine UI primitive systems
        ecs.createSystem<Simple2DObjectSystem>(masterRenderer);
        auto* ttfSys = ecs.createSystem<TTFTextSystem>(masterRenderer);
        ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Light.ttf", "inter", 18);

        auto* ui = ecs.createSystem<EditorUISystem>(masterRenderer, editor);

        // Execution ordering:
        //   EditorSystem first (updates editMode flag and blocks)
        //   Camera3DControllerEditor after EditorSystem (reads editMode)
        //   EditorUISystem after EditorSystem (reads editor state)
        //   MasterRenderer always runs last.
        ecs.succeed<Camera3DControllerEditor, EditorSystem>();
        ecs.succeed<EditorUISystem, EditorSystem>();
        ecs.succeed<MasterRenderer, Camera3DControllerEditor>();
        ecs.succeed<MasterRenderer, VoxelRenderSystem>();
        ecs.succeed<MasterRenderer, EditorUISystem>();

        // Build the initial floor on y=0
        editor->buildFloor();

        (void)ui;

        LOG_INFO(DOM, "Voxel editor ready — canvas "
                 << CANVAS_W << "x" << CANVAS_H << "x" << CANVAS_D
                 << " | Tab = edit mode | LMB = place | RMB = remove"
                 << " | Ctrl+Z/Y = undo/redo");
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
