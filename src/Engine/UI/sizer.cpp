#include "sizer.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Layout";
    }

    void LayoutSystem::init()
    {
        addListenerToStandardEvent("layoutScroll");

        auto vGroup = registerGroup<PositionComponent, VerticalLayout>();

        vGroup->addOnGroup([](EntityRef entity) {
            LOG_MILE(DOM, "Add entity " << entity->id << " to ui - vLayout group!");

            auto vLayout = entity->get<VerticalLayout>();
            auto position = entity->get<PositionComponent>();

            if (vLayout->scrollable)
                entity->world()->attach<MouseWheelComponent>(entity, StandardEvent{"layoutScroll", "id", entity->id});

            vLayout->visible = position->visible;
        });

        auto vClippedGroup = registerGroup<PositionComponent, VerticalLayout, ClippedTo>();

        vClippedGroup->addOnGroup([](EntityRef entity) {
            LOG_MILE(DOM, "Add entity " << entity->id << " to ui - vLayout - clipped group!");

            auto vLayout = entity->get<VerticalLayout>();
            auto clippedTo = entity->get<ClippedTo>();

            for (auto& ent : vLayout->entities)
            {
                entity->world()->attach<ClippedTo>(ent, clippedTo->clipperId);
            }
        });

        vClippedGroup->removeOfGroup([](EntitySystem* ecs, _unique_id id) {
            LOG_MILE(DOM, "Remove entity " << id << " of ui - vLayout - clipped group!");

            auto ent = ecs->getEntity(id);

            if (ent and ent->has<VerticalLayout>() and not ent->has<ClippedTo>())
            {
                auto vLayout = ent->get<VerticalLayout>();

                if (not vLayout->scrollable)
                {
                    for (auto& ent : vLayout->entities)
                    {
                        if (ent->template has<ClippedTo>())
                            ent->world()->detach<ClippedTo>(ent);
                    }
                }
            }
        });

        auto hGroup = registerGroup<PositionComponent, HorizontalLayout>();

        hGroup->addOnGroup([](EntityRef entity) {
            LOG_MILE(DOM, "Add entity " << entity->id << " to ui - hLayout group !");

            auto hLayout = entity->get<HorizontalLayout>();
            auto position = entity->get<PositionComponent>();

            if (hLayout->scrollable)
                entity->world()->attach<MouseWheelComponent>(entity, StandardEvent{"layoutScroll", "id", entity->id});

            hLayout->visible = position->visible;
        });

        auto hClippedGroup = registerGroup<PositionComponent, HorizontalLayout, ClippedTo>();

        hClippedGroup->addOnGroup([](EntityRef entity) {
            LOG_MILE(DOM, "Add entity " << entity->id << " to ui - hLayout - clipped group!");

            auto hLayout = entity->get<HorizontalLayout>();
            auto clippedTo = entity->get<ClippedTo>();
            
            for (auto& ent : hLayout->entities)
            {
                entity->world()->attach<ClippedTo>(ent, clippedTo->clipperId);
            }
        });

        hClippedGroup->removeOfGroup([](EntitySystem* ecs, _unique_id id) {
            LOG_MILE(DOM, "Remove entity " << id << " of ui - hLayout - clipped group!");

            auto ent = ecs->getEntity(id);

            if (ent and ent->has<HorizontalLayout>() and not ent->has<ClippedTo>())
            {
                auto hLayout = ent->get<HorizontalLayout>();

                if (not hLayout->scrollable)
                {
                    for (auto& ent : hLayout->entities)
                    {
                        if (ent->template has<ClippedTo>())
                            ent->world()->detach<ClippedTo>(ent);
                    }
                }
            }
        });
    }

    void LayoutSystem::onEvent(const StandardEvent& event)
    {
        auto id = event.values.at("id").get<size_t>();

        auto ent = ecsRef->getEntity(id);

        if (not ent)
        {
            LOG_ERROR(DOM, "Error while looking for entity: " << id);
            return;
        }

        float *offset;
        float scrollSpeed = 0.0f;

        float cW = 0.0f;
        float cH = 0.0f;

        if (ent->has<VerticalLayout>())
        {
            auto comp = ent->get<VerticalLayout>();
            offset = &comp->yOffset;
            scrollSpeed = comp->scrollSpeed;

            cW = comp->contentWidth;
            cH = comp->contentHeight;
        }
        else if (ent->has<HorizontalLayout>())
        {
            auto comp = ent->get<HorizontalLayout>();
            offset = &comp->xOffset;
            scrollSpeed = comp->scrollSpeed;

            cW = comp->contentWidth;
            cH = comp->contentHeight;
        }
        else
        {
            LOG_ERROR(DOM, "Error entity: " << id << " is not a layout!");
            return;
        }

        *offset -= event.values.at("y").get<int>() * scrollSpeed;

        LOG_ERROR(DOM, *offset << " cH: " << cH << " cW: " << cW);

        ecsRef->sendEvent(EntityChangedEvent{id});
    };


    void LayoutSystem::addEntity(EntityRef viewEnt, _unique_id ui, LayoutOrientation orientation)
    {
        if (not viewEnt->has<PositionComponent>() or not viewEnt->has<UiAnchor>())
        {
            LOG_ERROR(DOM, "Entity " << viewEnt.id << " must have a PositionComponent and UiAnchor!");
            return;
        }

        auto ent = ecsRef->getEntity(ui);

        // if (not ent or not ent->has<PositionComponent>() or not ent->has<UiAnchor>())
        // {
        //     LOG_ERROR(DOM, "Entity " << ui << " must have a PositionComponent and UiAnchor!");
        //     return;
        // }

        // Todo check if we really need to force children of a layout to have a UiAnchor just to be able to set the z constrain
        if (ent->has<UiAnchor>())
        {
            auto uiAnchor = ent->get<UiAnchor>();

            // Z + 1 so the initial z of the list is for the background
            uiAnchor->setZConstrain(PosConstrain{viewEnt.id, AnchorType::Z});
        }

        ecsRef->sendEvent(ParentingEvent{ui, viewEnt.id});

        if (orientation == LayoutOrientation::Horizontal)
        {
            auto view = viewEnt->get<HorizontalLayout>();

            // Todo need to make a special case if the vies is scrollable and clipped to something else
            if (view->scrollable)
            {
                ecsRef->attach<ClippedTo>(ent, view->id);
            }

            if (viewEnt->has<ClippedTo>())
            {
                ecsRef->attach<ClippedTo>(ent, viewEnt->get<ClippedTo>()->clipperId);
            }

            view->entities.push_back(ent);
        }
        else if (orientation == LayoutOrientation::Vertical)
        {
            auto view = viewEnt->get<VerticalLayout>();

            if (view->scrollable)
            {
                ecsRef->attach<ClippedTo>(ent, view->id);
            }

            if (viewEnt->has<ClippedTo>())
            {
                ecsRef->attach<ClippedTo>(ent, viewEnt->get<ClippedTo>()->clipperId);
            }

            view->entities.push_back(ent);
        }
        else
        {
            LOG_ERROR(DOM, "Invalid orientation for layout: " << static_cast<int>(orientation));
            return;
        }
    }

}