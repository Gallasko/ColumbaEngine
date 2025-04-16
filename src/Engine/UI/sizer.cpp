#include "sizer.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Layout";
    }

    void LayoutSystem::init()
    {
        auto vGroup = registerGroup<PositionComponent, VerticalLayout>();

        vGroup->addOnGroup([](EntityRef entity) {
            LOG_MILE(DOM, "Add entity " << entity->id << " to ui - vLayout group!");

            auto vLayout = entity->get<VerticalLayout>();
            auto position = entity->get<PositionComponent>();

            vLayout->visible = position->visible;
        });

        auto hGroup = registerGroup<PositionComponent, HorizontalLayout>();

        hGroup->addOnGroup([](EntityRef entity) {
            LOG_MILE(DOM, "Add entity " << entity->id << " to ui - hLayout group !");

            auto hLayout = entity->get<HorizontalLayout>();
            auto position = entity->get<PositionComponent>();

            hLayout->visible = position->visible;
        });
    }

    void LayoutSystem::addEntity(EntityRef viewEnt, _unique_id ui, LayoutOrientation orientation)
    {
        if (not viewEnt->has<PositionComponent>() or not viewEnt->has<UiAnchor>())
        {
            LOG_ERROR(DOM, "Entity " << viewEnt.id << " must have a PositionComponent and UiAnchor!");
            return;
        }

        auto ent = ecsRef->getEntity(ui);

        if (not ent or not ent->has<PositionComponent>() or not ent->has<UiAnchor>())
        {
            LOG_ERROR(DOM, "Entity " << ui << " must have a PositionComponent and UiAnchor!");
            return;
        }

        auto uiAnchor = ent->get<UiAnchor>();

        if (orientation == LayoutOrientation::Horizontal)
        {
            auto view = viewEnt->get<HorizontalLayout>();
            // Z + 1 so the initial z of the list is for the background
            uiAnchor->setZConstrain(PosConstrain{viewEnt.id, AnchorType::Z, PosOpType::Add, 1});

            view->entities.push_back(ent);
        }
        else if (orientation == LayoutOrientation::Vertical)
        {
            auto view = viewEnt->get<VerticalLayout>();
            // Z + 1 so the initial z of the list is for the background
            uiAnchor->setZConstrain(PosConstrain{viewEnt.id, AnchorType::Z, PosOpType::Add, 1});

            view->entities.push_back(ent);
        }
        else
        {
            LOG_ERROR(DOM, "Invalid orientation for layout: " << static_cast<int>(orientation));
            return;
        }
    }

}