#include "stdafx.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "application.h"

#include "window.h"
#include "Renderer/renderer.h"

#include "logger.h"

#include "Systems/basicsystems.h"
#include "Systems/oneventcomponent.h"

#include "Gui/contextmenu.h"
#include "Gui/inspector.h"
#include "Gui/projectmanager.h"
#include "Gui/entitylist.h"

#include "Scene/scenemanager.h"

#include "UI/ttftext.h"
#include "UI/namedanchor.h"

#include "UI/utils.h"

#include "Prefabs/foldablecard.h"

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

struct EntityFinder : public System<Listener<OnMouseClick>, Own<SelectedEntity>, Ref<PositionComponent>, Ref<SceneElement>, Ref<ResizeHandleComponent>, Ref<RotationHandleComponent>, InitSys>
{
    EntityRef selectionOutline;

    virtual void init() override
    {
        registerGroup<PositionComponent, SceneElement>();
        registerGroup<PositionComponent, ResizeHandleComponent>();
        registerGroup<PositionComponent, RotationHandleComponent>();

        auto outline = makeResizableSelectionOutline(ecsRef, 2.f, 8.f, {255.0f, 255.0f, 0.0f, 255.0f}, {255.0f, 255.0f, 255.0f, 255.0f}, true);

        outline.get<PositionComponent>()->setZ(25.f);

        selectionOutline = outline.entity;
        ecsRef->attach<EntityName>(selectionOutline, "SelectionOutline");
        ecsRef->_attach<SelectedEntity>(selectionOutline);
    }

    virtual void onEvent(const OnMouseClick& event) override
    {
        bool hit = false;

        // First, check for rotation handle clicks (highest priority)
        for (const auto& elem : viewGroup<PositionComponent, RotationHandleComponent>())
        {
            if (inClipBound(elem->entity, event.pos.x, event.pos.y))
            {
                hit = true;

                LOG_INFO(DOM, "Clicked on rotation handle");

                auto selectedId = selectionOutline.get<SelectedEntity>()->id;
                if (selectedId != 0)
                {
                    // Send rotation start event
                    ecsRef->sendEvent(StartRotation{ selectedId, elem->get<RotationHandleComponent>()->handle, event.pos.x, event.pos.y });
                }

                break;
            }
        }

        // Second, check for resize handle clicks
        if (not hit)
        {
            for (const auto& elem : viewGroup<PositionComponent, ResizeHandleComponent>())
        {
            if (inClipBound(elem->entity, event.pos.x, event.pos.y))
            {
                hit = true;

                LOG_INFO(DOM, "Clicked on resize handle: " << static_cast<int>(elem->get<ResizeHandleComponent>()->handle));

                auto selectedId = selectionOutline.get<SelectedEntity>()->id;
                if (selectedId != 0)
                {
                    // Send resize start event
                    ecsRef->sendEvent(StartResize{ selectedId, elem->get<ResizeHandleComponent>()->handle, event.pos.x, event.pos.y });
                }

                break;
            }
        }
        }

        // If no handles were clicked, scan all scene elements under the click
        if (not hit)
        {
            for (const auto& elem : viewGroup<PositionComponent, SceneElement>())
            {
                if (inClipBound(elem->entity, event.pos.x, event.pos.y))
            {
                hit = true;

                LOG_INFO(DOM, "Clicked on entity: " << elem->entityId);

                // send inspect event
                ecsRef->sendEvent(InspectEvent{ elem->entity });
                
                // send selection event to entity list
                ecsRef->sendEvent(SelectEntityEvent{ elem->entity.id });

                // position & size our outline to wrap this entity
                auto pos = elem->get<PositionComponent>();
                auto outlinePos = selectionOutline.get<PositionComponent>();
                outlinePos->setX(pos->x - 2.0f);
                outlinePos->setY(pos->y - 2.0f);
                outlinePos->setZ(pos->z + 1);
                outlinePos->setWidth(pos->width + 4.0f);
                outlinePos->setHeight(pos->height + 4.0f);
                outlinePos->setRotation(pos->rotation);

                // show it
                selectionOutline.get<Prefab>()->setVisibility(true);
                selectionOutline.get<SelectedEntity>()->id = elem->entity.id;

                break;
                }
            }
        }

        // If we clicked on empty space, hide the outline (and maybe clear inspect)
        if (not hit)
        {
            selectionOutline.get<Prefab>()->setVisibility(false);
        }
    }
};

struct DragSystem : public System<Listener<OnMouseClick>, Listener<OnMouseMove>, Listener<OnMouseRelease>, Listener<StartResize>, Listener<StartRotation>, Ref<PositionComponent>, Ref<SceneElement>, InitSys>
{
    _unique_id draggingEntity = 0;
    float offsetX = 0.f, offsetY = 0.f;
    bool lockX = false;  // Lock X-axis movement when left/right anchors are set
    bool lockY = false;  // Lock Y-axis movement when top/bottom anchors are set

    // Resize-related fields
    bool isResizing = false;
    _unique_id resizingEntity = 0;
    ResizeHandle activeHandle = ResizeHandle::None;
    float resizeStartWidth = 0.f, resizeStartHeight = 0.f, resizeStartX = 0.f, resizeStartY = 0.f;
    float initialMouseX = 0.f, initialMouseY = 0.f;

    // Rotation-related fields
    bool isRotating = false;
    _unique_id rotatingEntity = 0;
    RotationHandle activeRotationHandle = RotationHandle::None;
    float rotationStartAngle = 0.f;
    float entityCenterX = 0.f, entityCenterY = 0.f;
    
    float startX = 0.f, startY = 0.f;  // For dragging

    virtual std::string getSystemName() const override { return "Drag System"; }

    virtual void init() override
    {
        // we'll want to query all draggable scene elements
        registerGroup<PositionComponent, SceneElement>();
    }

    virtual void onEvent(const StartResize& e) override
    {
        LOG_INFO(DOM, "Starting resize on entity: " << e.entityId << " with handle: " << static_cast<int>(e.handle));
        
        isResizing = true;
        resizingEntity = e.entityId;
        activeHandle = e.handle;
        initialMouseX = e.startX;
        initialMouseY = e.startY;

        // Store initial dimensions
        auto pos = ecsRef->getComponent<PositionComponent>(e.entityId);
        if (pos)
        {
            resizeStartWidth = pos->width;
            resizeStartHeight = pos->height;
            resizeStartX = pos->x;
            resizeStartY = pos->y;
        }
    }

    virtual void onEvent(const StartRotation& e) override
    {
        LOG_INFO(DOM, "Starting rotation on entity: " << e.entityId);
        
        isRotating = true;
        rotatingEntity = e.entityId;
        activeRotationHandle = e.handle;
        initialMouseX = e.startX;
        initialMouseY = e.startY;

        // Store initial rotation and entity center
        auto pos = ecsRef->getComponent<PositionComponent>(e.entityId);
        if (pos)
        {
            rotationStartAngle = pos->rotation;
            entityCenterX = pos->x + pos->width / 2.0f;
            entityCenterY = pos->y + pos->height / 2.0f;
        }
    }

    virtual void onEvent(const OnMouseClick& e) override
    {
        // only start drag on left button
        if (e.button != SDL_BUTTON_LEFT)
            return;

        // Skip dragging if we're already resizing
        if (isResizing)
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
        if (isResizing)
        {
            performResize(e.pos.x, e.pos.y);
            return;
        }

        if (isRotating)
        {
            performRotation(e.pos.x, e.pos.y);
            return;
        }

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
            // Maintain rotation during drag
            outlinePos->setRotation(pos->rotation);
        }

        ecsRef->sendEvent(EntityChangedEvent{ draggingEntity });
    }

    void performResize(float mouseX, float mouseY)
    {
        auto pos = ecsRef->getComponent<PositionComponent>(resizingEntity);
        if (not pos)
            return;

        float deltaX = mouseX - initialMouseX;
        float deltaY = mouseY - initialMouseY;

        float newX = resizeStartX;
        float newY = resizeStartY;
        float newWidth = resizeStartWidth;
        float newHeight = resizeStartHeight;

        // Apply resize based on handle type
        switch (activeHandle)
        {
            case ResizeHandle::TopLeft:
                newX = resizeStartX + deltaX;
                newY = resizeStartY + deltaY;
                newWidth = resizeStartWidth - deltaX;
                newHeight = resizeStartHeight - deltaY;
                break;
            case ResizeHandle::Top:
                newY = resizeStartY + deltaY;
                newHeight = resizeStartHeight - deltaY;
                break;
            case ResizeHandle::TopRight:
                newY = resizeStartY + deltaY;
                newWidth = resizeStartWidth + deltaX;
                newHeight = resizeStartHeight - deltaY;
                break;
            case ResizeHandle::Left:
                newX = resizeStartX + deltaX;
                newWidth = resizeStartWidth - deltaX;
                break;
            case ResizeHandle::Right:
                newWidth = resizeStartWidth + deltaX;
                break;
            case ResizeHandle::BottomLeft:
                newX = resizeStartX + deltaX;
                newWidth = resizeStartWidth - deltaX;
                newHeight = resizeStartHeight + deltaY;
                break;
            case ResizeHandle::Bottom:
                newHeight = resizeStartHeight + deltaY;
                break;
            case ResizeHandle::BottomRight:
                newWidth = resizeStartWidth + deltaX;
                newHeight = resizeStartHeight + deltaY;
                break;
            default:
                return;
        }

        // Enforce minimum size
        constexpr float minSize = 10.0f;
        if (newWidth < minSize || newHeight < minSize)
            return;

        pos->setX(newX);
        pos->setY(newY);
        pos->setWidth(newWidth);
        pos->setHeight(newHeight);

        // Update selection outline
        auto ent = ecsRef->getEntity("SelectionOutline");
        if (ent && ent->get<SelectedEntity>()->id == resizingEntity)
        {
            auto outlinePos = ent->get<PositionComponent>();
            outlinePos->setX(newX - 2.f);
            outlinePos->setY(newY - 2.f);
            outlinePos->setWidth(newWidth + 4.f);
            outlinePos->setHeight(newHeight + 4.f);
            // Maintain rotation during resize
            outlinePos->setRotation(pos->rotation);
        }

        ecsRef->sendEvent(EntityChangedEvent{ resizingEntity });
    }

    void performRotation(float mouseX, float mouseY)
    {
        auto pos = ecsRef->getComponent<PositionComponent>(rotatingEntity);
        if (not pos)
            return;

        // Calculate angle from entity center to mouse position
        float deltaX = mouseX - entityCenterX;
        float deltaY = mouseY - entityCenterY;
        float currentAngle = std::atan2(deltaY, deltaX) * 180.0f / M_PI;  // Convert to degrees

        // Calculate initial angle from entity center to initial mouse position
        float initialDeltaX = initialMouseX - entityCenterX;
        float initialDeltaY = initialMouseY - entityCenterY;
        float initialAngle = std::atan2(initialDeltaY, initialDeltaX) * 180.0f / M_PI;  // Convert to degrees

        // Calculate rotation difference and apply to entity
        float angleDelta = currentAngle - initialAngle;
        float newRotation = rotationStartAngle + angleDelta;

        // Normalize rotation to 0-360 degrees
        while (newRotation < 0.0f) newRotation += 360.0f;
        while (newRotation >= 360.0f) newRotation -= 360.0f;

        pos->setRotation(newRotation);

        // Update selection outline rotation
        auto ent = ecsRef->getEntity("SelectionOutline");
        if (ent && ent->get<SelectedEntity>()->id == rotatingEntity)
        {
            auto outlinePos = ent->get<PositionComponent>();
            outlinePos->setRotation(newRotation);
        }

        ecsRef->sendEvent(EntityChangedEvent{ rotatingEntity });
    }

    virtual void onEvent(const OnMouseRelease& e) override
    {
        // only stop drag on left button
        if (e.button != SDL_BUTTON_LEFT)
            return;

        if (isResizing)
        {
            auto pos = ecsRef->getComponent<PositionComponent>(resizingEntity);
            if (pos)
            {
                // send event to notify that resizing has ended
                ecsRef->sendEvent(EndResize{ resizingEntity, activeHandle, 
                    resizeStartWidth, resizeStartHeight, resizeStartX, resizeStartY,
                    pos->width, pos->height, pos->x, pos->y });
            }

            isResizing = false;
            resizingEntity = 0;
            activeHandle = ResizeHandle::None;
        }

        if (isRotating)
        {
            auto pos = ecsRef->getComponent<PositionComponent>(rotatingEntity);
            if (pos)
            {
                // send event to notify that rotation has ended
                ecsRef->sendEvent(EndRotation{ rotatingEntity, activeRotationHandle, 
                    rotationStartAngle, pos->rotation });
            }

            isRotating = false;
            rotatingEntity = 0;
            activeRotationHandle = RotationHandle::None;
        }

        if (draggingEntity != 0)
        {
            auto pos = ecsRef->getComponent<PositionComponent>(draggingEntity);
            if (not pos) return;

            // send event to notify that dragging has ended
            ecsRef->sendEvent(EndDragging{ draggingEntity, startX, startY, pos->x, pos->y });
        }

        draggingEntity = 0;
    }
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

        inspector->registerCustomDrawer("Entity", [](InspectorSystem* sys, SerializedInfoHolder& parent, CompRef<VerticalLayout> view) {
            LOG_INFO("Inspector", "Custom drawer for Entity");

            for (auto& child : parent.children)
            {
                if (child.className != "")
                {
                    sys->printChildren(child, view);
                }
                else
                {
                    LOG_INFO("Inspector", "Entity: " << child.name);
                }
            }
        });

        inspector->registerCustomDrawer<UiAnchor>([](InspectorSystem*, SerializedInfoHolder&, CompRef<VerticalLayout>) {
            LOG_ERROR("Inspector", "Todo ! : Custom drawer for UiAnchor, right now it skips it entirely");
        });

        inspector->registerCustomDrawer<NamedUiAnchor>([](InspectorSystem* sys, SerializedInfoHolder& parent, CompRef<VerticalLayout> view) {
            // If no class name then we got an attribute
            if (parent.className == "")
            {
                sys->addNewAttribute(parent.name, parent.value, view);
            }
            // We got a class name then it is a class ! So no type nor value
            else
            {
                view = sys->addNewText(parent.className, view);
            }

            // auto ent = make9squarePrefab(sys->ecsRef);

            // sys->view->addEntity(makeFoldableCard(sys->ecsRef));

            // LOG_INFO("Inspector", "Ent: " << ent.id);

            // sys->view->addEntity(ent);

            // Process children but skip internal flags
            for (auto& child : parent.children)
            {
                // Skip all the internal has* flags
                if (child.name == "hasTopAnchor"        or
                    child.name == "hasLeftAnchor"       or
                    child.name == "hasRightAnchor"      or
                    child.name == "hasBottomAnchor"     or
                    child.name == "hasVerticalCenter"   or
                    child.name == "hasHorizontalCenter" or
                    child.name == "hasWidthConstrain"   or
                    child.name == "hasHeightConstrain"  or
                    child.name == "hasZConstrain")
                {
                    continue; // Skip these internal flags
                }

                // Draw everything else normally
                sys->printChildren(child, view);
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
        ecs.createSystem<EntityListSystem>();

        ecs.createSystem<FoldCardSystem>();
    });
}

EditorApp::~EditorApp()
{
}

int EditorApp::exec()
{
    return engine.exec();
}
