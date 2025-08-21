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
#include "UI/namedanchor.h"

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
    bool lockX = false;  // Lock X-axis movement when left/right anchors are set
    bool lockY = false;  // Lock Y-axis movement when top/bottom anchors are set

    virtual std::string getSystemName() const override { return "Drag System"; }

    virtual void init() override
    {
        // we’ll want to query all draggable scene elements
        registerGroup<PositionComponent, SceneElement>();
    }

    virtual void onEvent(const OnMouseClick& e) override
    {
        // only start drag on left button
        if (e.button != SDL_BUTTON_LEFT)
            return;

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

                // Check for UiAnchor constraints
                lockX = false;
                lockY = false;

                auto anchor = elem->entity->get<UiAnchor>();
                if (anchor)
                {
                    // If top or bottom anchor is set, lock Y movement
                    if (anchor->hasTopAnchor || anchor->hasBottomAnchor)
                    {
                        lockY = true;
                        LOG_INFO(DOM, "Y-axis locked due to top/bottom anchor");
                    }

                    // If left or right anchor is set, lock X movement
                    if (anchor->hasLeftAnchor || anchor->hasRightAnchor)
                    {
                        lockX = true;
                        LOG_INFO(DOM, "X-axis locked due to left/right anchor");
                    }

                    if (lockX && lockY)
                    {
                        LOG_INFO(DOM, "Entity fully constrained - no dragging allowed");
                    }
                }

                break;
            }
        }
    }

    virtual void onEvent(const OnMouseMove& e) override
    {
        if (draggingEntity == 0)
            return;

        // update position each frame
        auto pos = ecsRef->getComponent<PositionComponent>(draggingEntity);
        if (not pos)
            return;

        // Apply movement constraints based on UiAnchor settings
        float newX = lockX ? pos->x : (e.pos.x - offsetX);
        float newY = lockY ? pos->y : (e.pos.y - offsetY);

        pos->setX(newX);
        pos->setY(newY);

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
        if (e.button != SDL_BUTTON_LEFT)
            return;

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

EntityRef make9squarePrefab(EntitySystem* ecsRef)
{
    float margin = 5;

    auto prefabEnt = makeAnchoredPrefab(ecsRef, 10, 10, 1);
    auto prefab = prefabEnt.get<Prefab>();

    auto bg = makeUiSimple2DShape(ecsRef, Shape2D::Square, 150 + margin * 4, 150 + margin * 4, {155.0, 55.0, 55.0, 255.0});
    auto bgAnchor = bg.get<UiAnchor>();

    prefab->setMainEntity(bg);

    auto top  = bgAnchor->top;
    auto left = bgAnchor->left;

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            auto sq = makeUiSimple2DShape(ecsRef, Shape2D::Square, 50, 50);
            auto sqAnchor = sq.get<UiAnchor>();
            sq.get<PositionComponent>()->setZ(10);

            sqAnchor->setTopAnchor(top);
            sqAnchor->setTopMargin(margin + (50 + margin) * i);
            sqAnchor->setLeftAnchor(left);
            sqAnchor->setLeftMargin(margin + (50 + margin) * j);

            prefab->addToPrefab(sq);
        }
    }

    return prefabEnt;
}

struct FoldCardEvent
{
    _unique_id id;
};

struct FoldCardSystem : public System<Listener<FoldCardEvent>>
{
    void onEvent(const FoldCardEvent& event)
    {
        auto ent = ecsRef->getEntity(event.id);

        if (not ent or not ent->has<Prefab>())
            return;

        ent->get<Prefab>()->callHelper("toggleCard");
    }
};

CompList<Prefab, UiAnchor, VerticalLayout> makeFoldableCard(EntitySystem* ecsRef)
{
    auto prefabEnt = makeAnchoredPrefab(ecsRef, 100, 100, 1);
    auto prefab = prefabEnt.get<Prefab>();
    auto prefabAnchor = prefabEnt.get<UiAnchor>();

    auto mainLayoutEnt = makeVerticalLayout(ecsRef, 0, 0, 250, 0);
    auto mainLayout = mainLayoutEnt.get<VerticalLayout>();
    auto mainLayoutAnchor = mainLayoutEnt.get<UiAnchor>();
    mainLayout->setScrollable(false);

    prefabAnchor->setWidthConstrain(PosConstrain{mainLayout.entityId, AnchorType::Width});
    prefabAnchor->setHeightConstrain(PosConstrain{mainLayout.entityId, AnchorType::Height});

    mainLayoutAnchor->setTopAnchor(prefabAnchor->top);
    mainLayoutAnchor->setLeftAnchor(prefabAnchor->left);
    mainLayoutAnchor->setZConstrain(PosConstrain{prefabEnt.id, AnchorType::Z});

    prefab->addToPrefab(mainLayoutEnt, "MainEntity");

    auto titleBg = makeUiSimple2DShape(ecsRef, Shape2D::Square, 250, 50, {55, 55, 125, 255});
    titleBg.attach<MouseLeftClickComponent>(makeCallable<FoldCardEvent>(FoldCardEvent{prefabEnt.id}));
    auto titleBgAnchor = titleBg.get<UiAnchor>();

    auto title = makeTTFText(ecsRef, 0, 0, 2, "light", "Card Name", 0.4f);
    auto titleAnchor = title.get<UiAnchor>();

    titleAnchor->setVerticalCenter(titleBgAnchor->verticalCenter);
    titleAnchor->setLeftAnchor(titleBgAnchor->left);
    titleAnchor->setLeftMargin(5);

    auto layoutEnt = makeVerticalLayout(ecsRef, 0, 0, 250, 100);
    auto layout = layoutEnt.get<VerticalLayout>();
    // layout->fitToAxis = true;
    layout->setScrollable(false);
    auto layoutAnchor = layoutEnt.get<UiAnchor>();

    auto test1 = makeTTFText(ecsRef, 0, 0, 2, "light", "Test 1", 0.5);
    auto test2 = makeTTFText(ecsRef, 0, 0, 2, "light", "Test 2", 0.5);

    layout->addEntity(test1);
    layout->addEntity(test2);

    mainLayout->addEntity(titleBg);
    mainLayout->addEntity(layoutEnt);

    prefab->addHelper("toggleCard", [](Prefab *prefab) -> void {
        LOG_INFO("Foldable card", "Fold");
        auto vLayoutEnt = prefab->getEntity("MainEntity")->get<VerticalLayout>()->entities[1];

        auto vLayoutPos = vLayoutEnt->get<PositionComponent>();

        auto visible = vLayoutPos->isVisible();

        vLayoutPos->setVisibility(not visible);
    });

    return {prefabEnt, prefab, prefabAnchor, layout};
}

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

        inspector->registerCustomDrawer<NamedUiAnchor>([](InspectorSystem* sys, SerializedInfoHolder& parent) {
            // If no class name then we got an attribute
            if (parent.className == "")
            {
                sys->addNewAttribute(parent.name, parent.type, parent.value);
            }
            // We got a class name then it is a class ! So no type nor value
            else
            {
                sys->addNewText(parent.className);
            }

            auto ent = make9squarePrefab(sys->ecsRef);

            sys->view->addEntity(makeFoldableCard(sys->ecsRef));

            LOG_INFO("Inspector", "Ent: " << ent.id);

            sys->view->addEntity(ent);

            // Process children but skip internal flags
            for (auto& child : parent.children)
            {
                // Skip all the internal has* flags
                if (child.name == "hasTopAnchor" ||
                    child.name == "hasLeftAnchor" ||
                    child.name == "hasRightAnchor" ||
                    child.name == "hasBottomAnchor" ||
                    child.name == "hasVerticalCenter" ||
                    child.name == "hasHorizontalCenter" ||
                    child.name == "hasWidthConstrain" ||
                    child.name == "hasHeightConstrain" ||
                    child.name == "hasZConstrain")
                {
                    continue; // Skip these internal flags
                }

                // Draw everything else normally
                sys->printChildren(child);
            }
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

        ecs.createSystem<FoldCardSystem>();
        auto fCard = makeFoldableCard(&ecs);
        auto fCardAnchor = fCard.get<UiAnchor>();

        auto testSquare = makeUiSimple2DShape(&ecs, Shape2D::Square, 40, 40);
        auto testSquareAnchor = testSquare.get<UiAnchor>();

        testSquareAnchor->setTopAnchor(fCardAnchor->bottom);
        testSquareAnchor->setLeftAnchor(fCardAnchor->left);
    });
}

EditorApp::~EditorApp()
{
}

int EditorApp::exec()
{
    return engine.exec();
}
