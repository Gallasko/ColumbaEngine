#include "stdafx.h"

#include "Init/uisystems.h"

#include "ECS/entitysystem.h"
#include "2D/position.h"
#include "UI/namedanchor.h"
#include "UI/animation.h"
#include "UI/prefab.h"
#include "UI/sizer.h"
#include "UI/listview.h"
#include "UI/progressbar.h"
#include "UI/textinput.h"
#include "Scene/scenemanager.h"
#include "Renderer/renderer.h"

namespace pg
{
    void registerUiSystems(EntitySystem* ecs)
    {
        ecs->createSystem<PositionComponentSystem>();
        ecs->createSystem<NamedUiAnchorSystem>();
        ecs->createSystem<AnimationPositionSystem>();
        ecs->createSystem<SceneElementSystem>();
        ecs->createSystem<PrefabSystem>();
        ecs->createSystem<LayoutSystem>();
        ecs->createSystem<ListViewSystem>();

        // Ordering
        ecs->succeed<LayoutSystem, PrefabSystem>();

        ecs->succeed<PositionComponentSystem, PrefabSystem>();
        ecs->succeed<PositionComponentSystem, NamedUiAnchorSystem>();
        ecs->succeed<PositionComponentSystem, ProgressBarComponentSystem>();
        ecs->succeed<PositionComponentSystem, ListViewSystem>();
        ecs->succeed<PositionComponentSystem, LayoutSystem>();
        ecs->succeed<PositionComponentSystem, TextInputComponent>();

        ecs->succeed<AnimationPositionSystem, PositionComponentSystem>();

        ecs->succeed<SceneElementSystem, MasterRenderer>();
    }
}
