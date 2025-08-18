#include "stdafx.h"

#include "application.h"

#include "window.h"
#include "Renderer/renderer.h"

#include "logger.h"

#include "Systems/basicsystems.h"
#include "Systems/oneventcomponent.h"

#include "Gui/contextmenu.h"
#include "Gui/inspector.h"
#include "Gui/projectmanager.h"

#include "Scene/scenemanager.h"

#include "UI/ttftext.h"

#include "UI/utils.h"

using namespace pg;
using namespace editor;

namespace
{
    static const char* const DOM = "Editor app";
}

struct SelectedEntity
{
    SelectedEntity() : id(0) {}
    SelectedEntity(_unique_id id) : id(id) {}

    _unique_id id;
};

struct EntityFinder : public System<Listener<OnMouseClick>, Own<SelectedEntity>, Ref<PositionComponent>, Ref<SceneElement>, InitSys>
{
    EntityRef selectionOutline;

    virtual void init() override
    {
        registerGroup<PositionComponent, SceneElement>();

        auto outline = makeSelectionOutlinePrefab(ecsRef, 2.f, {255.0f, 255.0f, 0.0f, 255.0f}, false);

        outline.get<PositionComponent>()->setZ(25.f);

        selectionOutline = outline.entity;
        ecsRef->attach<EntityName>(selectionOutline, "SelectionOutline");
        ecsRef->_attach<SelectedEntity>(selectionOutline);
    }

    virtual void onEvent(const OnMouseClick& event) override
    {
        bool hit = false;

        // scan all scene elements under the click
        for (const auto& elem : viewGroup<PositionComponent, SceneElement>())
        {
            if (inClipBound(elem->entity, event.pos.x, event.pos.y))
            {
                hit = true;

                LOG_INFO(DOM, "Clicked on entity: " << elem->entityId);

                // send inspect event
                ecsRef->sendEvent(InspectEvent{ elem->entity });

                // position & size our outline to wrap this entity
                auto pos = elem->get<PositionComponent>();
                auto outlinePos = selectionOutline.get<PositionComponent>();
                outlinePos->setX(pos->x - 2.0f);
                outlinePos->setY(pos->y - 2.0f);
                outlinePos->setWidth(pos->width + 4.0f);
                outlinePos->setHeight(pos->height + 4.0f);

                // show it
                selectionOutline.get<Prefab>()->setVisibility(true);
                selectionOutline.get<SelectedEntity>()->id = elem->entity.id;

                break;
            }
        }

        // If we clicked on empty space, hide the outline (and maybe clear inspect)
        if (not hit)
        {
            selectionOutline.get<Prefab>()->setVisibility(false);
        }
    }
};

struct DragSystem : public System<Listener<OnMouseClick>, Listener<OnMouseMove>, Listener<OnMouseRelease>, Ref<PositionComponent>, Ref<SceneElement>, InitSys>
{
    _unique_id draggingEntity = 0;
    float offsetX = 0.f, offsetY = 0.f;

    virtual std::string getSystemName() const override { return "Drag System"; }

    virtual void init() override
    {
        // we’ll want to query all draggable scene elements
        registerGroup<PositionComponent, SceneElement>();
    }

    virtual void onEvent(const OnMouseClick& e) override
    {
        // only start drag on left button
        if (e.button != SDL_BUTTON_LEFT) return;

        // find topmost element under cursor
        for (const auto& elem : viewGroup<PositionComponent, SceneElement>())
        {
            auto pos = elem->get<PositionComponent>();
            if (inClipBound(elem->entity, e.pos.x, e.pos.y))
            {
                LOG_INFO(DOM, "Dragging entity: " << elem->entityId);
                draggingEntity = elem->entity.id;

                startX = pos->x;
                startY = pos->y;

                // remember offset so entity doesn't jump under cursor
                offsetX = e.pos.x - pos->x;
                offsetY = e.pos.y - pos->y;
                break;
            }
        }
    }

    virtual void onEvent(const OnMouseMove& e) override
    {
        if (draggingEntity == 0) return;

        // update position each frame
        auto pos = ecsRef->getComponent<PositionComponent>(draggingEntity);
        if (not pos) return;

        pos->setX(e.pos.x - offsetX);
        pos->setY(e.pos.y - offsetY);

        auto ent = ecsRef->getEntity("SelectionOutline");
        if (not ent) return;

        if (ent->get<SelectedEntity>()->id == draggingEntity)
        {
            auto outlinePos = ent->get<PositionComponent>();

            outlinePos->setX(pos->x - 2.f);
            outlinePos->setY(pos->y - 2.f);
            outlinePos->setWidth(pos->width + 4.f);
            outlinePos->setHeight(pos->height + 4.f);
        }

        ecsRef->sendEvent(EntityChangedEvent{ draggingEntity });
    }

    virtual void onEvent(const OnMouseRelease& e) override
    {
        // only stop drag on left button
        if (e.button != SDL_BUTTON_LEFT) return;

        if (draggingEntity != 0)
        {
            auto pos = ecsRef->getComponent<PositionComponent>(draggingEntity);
            if (not pos) return;

            // send event to notify that dragging has ended
            ecsRef->sendEvent(EndDragging{ draggingEntity, startX, startY, pos->x, pos->y });
        }

        draggingEntity = 0;
    }

    float startX = 0.f, startY = 0.f;
};

EditorApp::EditorApp(const std::string &appName) : engine(appName)
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        auto ttfSys = ecs.createSystem<TTFTextSystem>(window.masterRenderer);

        #ifdef __EMSCRIPTEN__
            // Need to fix this
            ttfSys->registerFont("/res/font/Inter/static/Inter_28pt-Light.ttf", "light");
            ttfSys->registerFont("/res/font/Inter/static/Inter_28pt-Bold.ttf", "bold");
            ttfSys->registerFont("/res/font/Inter/static/Inter_28pt-Italic.ttf", "italic");
        #else
            ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Light.ttf", "light");
            ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Bold.ttf", "bold");
            ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Italic.ttf", "italic");
        #endif

        window.masterRenderer->processTextureRegister();

        ecs.createSystem<ConfiguredKeySystem<EditorKeyConfig>>(scancodeMap);

        ecs.createSystem<FpsSystem>();

        ecs.createSystem<MoveToSystem>();
        ecs.createSystem<ContextMenu>();

        auto inspector = ecs.createSystem<InspectorSystem>();

        inspector->registerCustomDrawer("Entity", [](InspectorSystem* sys, SerializedInfoHolder& parent) {
            LOG_INFO("Inspector", "Custom drawer for Entity");

            for (auto& child : parent.children)
            {
                if (child.className != "")
                {
                    sys->printChildren(child);
                }
                else
                {
                    LOG_INFO("Inspector", "Entity: " << child.name);
                }
            }
        });

        inspector->registerCustomDrawer<UiAnchor>([](InspectorSystem*, SerializedInfoHolder&) {
            LOG_ERROR("Inspector", "Todo ! : Custom drawer for UiAnchor, right now it skips it entirely");
        });

        // mainWindow->ecs.succeed<InspectorSystem, ListViewSystem>();
        ecs.succeed<MasterRenderer, TTFTextSystem>();

        auto ent = ecs.createEntity();

        std::function callback = [&ecs, &window](const OnMouseClick& event) {
            if (event.button == SDL_BUTTON_RIGHT)
            {
                ecs.sendEvent(ShowContextMenu{window.getInputHandler()});
            }
        };

        ecs.attach<OnEventComponent>(ent, callback);

        ecs.createSystem<EntityFinder>();
        ecs.createSystem<DragSystem>();

    });
}

EditorApp::~EditorApp()
{
}

int EditorApp::exec()
{
    return engine.exec();
}
