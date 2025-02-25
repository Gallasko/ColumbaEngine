#pragma once

#include "ECS/system.h"

#include "uisystem.h"

#include "2D/texture.h"

#include "focusable.h"

#include "Systems/oneventcomponent.h"

#include "Input/inputcomponent.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <SDL2/SDL.h>
#include <SDL_opengles2.h>
// #include <SDL_opengl_glext.h>
// #include <GLES2/gl2.h>
// #include <GLFW/glfw3.h>
#else
    #ifdef __linux__
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_opengl.h>
    #elif _WIN32
    #include <SDL.h>
    #include <SDL_opengl.h>
    #endif
#endif

namespace pg
{
    struct ClearListViewEvent
    {
        _unique_id id;
    };

    struct AddListViewElementEvent
    {
        _unique_id id; _unique_id ui;
    };

    struct RemoveListViewElementEvent
    {
        _unique_id id; size_t index;
    };

    struct ListViewElement : public Dtor 
    {
        ListViewElement(_unique_id viewId, size_t index) : viewId(viewId), index(index) {}
        ListViewElement(const ListViewElement& other) : viewId(other.viewId), index(other.index) {}

        virtual void onDeletion(EntityRef entity) override
        {
            entity.ecsRef->sendEvent(RemoveListViewElementEvent{entity.id, index});
        };

        /** Id of the list view entity */
        _unique_id viewId = 0;

        /** Position of this entity inside the list view */
        size_t index = 0;
    };

    struct ListView : public Ctor
    {
        void onCreation(EntityRef entity) override
        {
            id = entity.id;
            ecsRef = entity.ecsRef;
        }

        void addEntity(EntityRef entity) { ecsRef->sendEvent(AddListViewElementEvent{id, entity.id}); }

        void removeEntity(EntityRef entity);

        void updateVisibility();

        // Todo send an event for visibility update
        void setVisibility(bool visible);

        void clear() { ecsRef->sendEvent(ClearListViewEvent{id}); }

        // Todo currently only supports vertical list view
        // Todo make it work for horizontal or both hori/verti slider (2 sliders at the same time)
        EntityRef cursor;

        float cursorHeight;
        float listReelHeight;

        EntityRef slider;

        /** Spacing between each entity of the list */
        float spacing = 5;

        /** Flag indicating whether the list must show the last item in the list when adding a new entity */
        bool stickToBottom = false;

        std::vector<EntityRef> entities;

        _unique_id id;

        EntitySystem *ecsRef;
    };

    struct ListViewSystem : public System<Listener<AddListViewElementEvent>, Listener<ClearListViewEvent>, Own<ListView>, InitSys>
    {
        virtual std::string getSystemName() const override { return "ListView System"; }

        virtual void init() override;

        virtual void onEvent(const AddListViewElementEvent& event) override
        {
            eventQueue.push(event);
        }

        virtual void onEvent(const ClearListViewEvent& event) override
        {
            clearQueue.push(event);
        }

        virtual void execute() override
        {
            while (not clearQueue.empty())
            {
                const auto& event = clearQueue.front();

                auto ent = ecsRef->getEntity(event.id);

                if (not (ent->has<ListView>()))
                {
                    LOG_ERROR("ListView", "Entity requested doesn't have a list view component !");
                    return;
                }

                clear(ent->get<ListView>());

                clearQueue.pop();
            }

            while (not eventQueue.empty())
            {
                const auto& event = eventQueue.front();

                auto ent = ecsRef->getEntity(event.id);

                if (not (ent->has<ListView>()))
                {
                    LOG_ERROR("ListView", "Entity requested doesn't have a list view component !");
                    return;
                }

                addEntity(ent, event.ui);

                eventQueue.pop();
            }
        }

        void addEntity(EntityRef viewEnt, _unique_id ui);

        void removeEntity(EntityRef /* entity */) {}

        void calculateListSize(CompRef<ListView> view);

        void updateCursorSize(CompRef<ListView> view, const UiSize& maxPos);

        void clear(CompRef<ListView> view);

        std::queue<AddListViewElementEvent> eventQueue;

        std::queue<ClearListViewEvent> clearQueue;
    };

    /** Helper that create an entity with an Ui component and a Texture component */
    template <typename Type>
    CompList<PositionComponent, UiAnchor, ListView> makeListView(Type *ecs, float x, float y, float width, float height)
    {
        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        auto view = ecs->template attach<ListView>(entity);

        ui->setX(x);
        ui->setY(y);

        ui->setWidth(width);
        ui->setHeight(height);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        // Z + 3 so the cursor is always on top of the slider
        auto cursor = makeUiTexture(ecs, 15, 40, "cursor");
        auto cursorAnchor = cursor.template get<UiAnchor>();

        cursorAnchor->setZConstrain(PosConstrain{entity->id, AnchorType::Z, PosOpType::Add, 3});
        cursorAnchor->setHeightConstrain(PosConstrain{entity->id, AnchorType::Height});
        cursorAnchor->setTopAnchor(anchor->top);
        cursorAnchor->setRightAnchor(anchor->right);

        ecs->template attach<FocusableComponent>(cursor.entity);

        auto viewId = view.entityId;

        auto cursorId = cursor.entity.id;

        // Todo improve this (should be possible with the new constrain system for position)
        // Todo add this back first
        std::function<void(const OnMouseMove&)> cursorCallback = [viewId, ecs](const OnMouseMove event) {
            if (not event.inputHandler->isButtonPressed(SDL_BUTTON_LEFT))
                return;

            auto ent = ecs->getEntity(viewId);

            if (not ent)
                return;

            auto viewComp = ent->template get<ListView>();
            auto viewUi = ent->template get<PositionComponent>();

            auto focus = viewComp->cursor->template get<FocusableComponent>();
            auto cursorUi = viewComp->cursor->template get<PositionComponent>();
            auto cursorAnchor = viewComp->cursor->template get<UiAnchor>();

            if (not focus->focused)
                return;

            float cHeight = viewComp->cursorHeight;

            float currentPos = event.pos.y - viewUi->y - cHeight / 2.0f;

            if (currentPos < 0)
                currentPos = 0;

            float maxHeight = viewUi->height - cursorUi->height;

            if (currentPos > maxHeight)
                currentPos = maxHeight;

            cursorAnchor->setTopMargin(currentPos);

            if (viewComp->entities.size() > 0)
            {
                if (viewComp->entities[0]->template has<UiAnchor>())
                {
                    viewComp->entities[0]->template get<UiAnchor>()->setTopMargin(-currentPos * viewComp->listReelHeight / viewUi->height);
                }
                else
                {
                    LOG_ERROR("ListViewSystem", "First entity in the list [" << viewId << "] must have a UiAnchor!");
                }   
            }

            viewComp->updateVisibility();
        };

        ecs->template attach<OnEventComponent>(cursor.entity, cursorCallback);

        ecs->template attach<MouseLeftClickComponent>(cursor.entity, makeCallable<OnFocus>(cursorId), MouseStateTrigger::OnPress);

        // Z + 2 so the slider is always on top of any entity in the list
        auto slider = makeUiTexture(ecs, 15, 1, "slider");
        auto sliderAnchor = slider.template get<UiAnchor>();

        sliderAnchor->setZConstrain(PosConstrain{entity->id, AnchorType::Z, PosOpType::Add, 2});
        sliderAnchor->setTopAnchor(anchor->top);
        sliderAnchor->setBottomAnchor(anchor->bottom);
        sliderAnchor->setRightAnchor(anchor->right);

        view->cursor = cursor.entity;

        view->slider = slider.entity;

        return CompList<PositionComponent, UiAnchor, ListView>(entity, ui, anchor, view);
    }

}