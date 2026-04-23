#include "stdafx.h"

#include "Init/rendersystems.h"

#include "ECS/entitysystem.h"
#include "Renderer/renderer.h"
#include "Renderer/renderermodule.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "2D/position.h"
#include "UI/progressbar.h"
#include "UI/prefab.h"
#include "Interpreter/pginterpreter.h"

namespace pg
{
    MasterRenderer* registerRenderSystems(EntitySystem* ecs, PgInterpreter* interpreter, int width, int height)
    {
        auto* masterRenderer = ecs->createSystem<MasterRenderer>("res/None.png");
        interpreter->addSystemModule("renderer", RendererModule{masterRenderer});
        interpreter->interpretFromFile("res/setupRenderer.pg");
        masterRenderer->setWindowSize(width, height);

        ecs->createSystem<Simple2DObjectSystem>(masterRenderer);
        ecs->createSystem<RoundedRect2DObjectSystem>(masterRenderer);
        ecs->createSystem<Texture2DComponentSystem>(masterRenderer);
        ecs->createSystem<ProgressBarComponentSystem>(masterRenderer);

        // Ordering: MasterRenderer runs after all sub-renderers
        ecs->succeed<MasterRenderer, Simple2DObjectSystem>();
        ecs->succeed<MasterRenderer, RoundedRect2DObjectSystem>();
        ecs->succeed<MasterRenderer, Texture2DComponentSystem>();
        ecs->succeed<MasterRenderer, ProgressBarComponentSystem>();
        ecs->succeed<MasterRenderer, PrefabSystem>();
        ecs->succeed<MasterRenderer, PositionComponentSystem>();

        return masterRenderer;
    }
}
