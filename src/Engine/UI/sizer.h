#pragma once

#include "ECS/system.h"

#include "2D/texture.h"

namespace pg
{
    enum class LayoutOrientation
    {
        Horizontal,
        Vertical
    };

    struct ClearLayoutEvent
    {
        _unique_id id; LayoutOrientation orientation;
    };

    struct AddLayoutElementEvent
    {
        _unique_id id; _unique_id ui; LayoutOrientation orientation;
    };

    struct RemoveLayoutElementEvent
    {
        _unique_id id; _unique_id index; LayoutOrientation orientation;
    };

    struct UpdateLayoutVisibility
    {
        _unique_id id; bool visible; LayoutOrientation orientation;
    };

    template <typename Layout>
    size_t getNbVisibleElementsInLayout(Layout layout)
    {
        size_t nb = 0;

        for (auto& ent : layout->entities)
        {
            if (ent->template has<PositionComponent>())
            {
                auto pos = ent->template get<PositionComponent>();

                if (pos->visible)
                {
                    nb++;
                }
            }
        }

        return nb;
    }

    struct BaseLayout : public Ctor
    {
        virtual void onCreation(EntityRef entity) override
        {
            id = entity.id;
            ecsRef = entity.ecsRef;
        }

        void addEntity(EntityRef entity)
        {
            ecsRef->sendEvent(AddLayoutElementEvent{id, entity.id, orientation});
        }

        void removeEntity(EntityRef entity)
        {
            ecsRef->sendEvent(RemoveLayoutElementEvent{id, entity.id, orientation});
        }

        void removeEntity(_unique_id entityId)
        {
            ecsRef->sendEvent(RemoveLayoutElementEvent{id, entityId, orientation});
        }

        void setVisibility(bool vis)
        {
            ecsRef->sendEvent(UpdateLayoutVisibility{id, vis, orientation});
        }

        void clear()
        {
            ecsRef->sendEvent(ClearLayoutEvent{id, orientation});
        }

        bool fitToAxis = false;
        bool spaced = false;
        size_t spacing = 0;
        bool visible = true;
        bool sizedToContent = true;

        LayoutOrientation orientation = LayoutOrientation::Horizontal;

        std::vector<EntityRef> entities;

        _unique_id id;
        EntitySystem *ecsRef;
    };


    struct HorizontalLayout : public BaseLayout
    {
        HorizontalLayout() : BaseLayout()
        {
            orientation = LayoutOrientation::Horizontal;
        }
    };

    struct VerticalLayout : public BaseLayout
    {
        VerticalLayout() : BaseLayout()
        {
            orientation = LayoutOrientation::Vertical;
        }
    };

    struct LayoutSystem : public System<
        Listener<EntityChangedEvent>,
        Listener<AddLayoutElementEvent>,
        Listener<RemoveLayoutElementEvent>,
        Listener<UpdateLayoutVisibility>,
        Listener<ClearLayoutEvent>,
        Own<HorizontalLayout>,
        Own<VerticalLayout>,
        InitSys>
    {
        virtual std::string getSystemName() const override { return "Layout System"; }

        virtual void init() override;

        virtual void onEvent(const AddLayoutElementEvent& event) override
        {
            addQueue.push(event);
        }

        virtual void onEvent(const RemoveLayoutElementEvent& event) override
        {
            removeQueue.push(event);
        }

        virtual void onEvent(const UpdateLayoutVisibility& event) override
        {
            visibilityQueue.push(event);
        }

        virtual void onEvent(const ClearLayoutEvent& event) override
        {
            clearQueue.push(event);
        }

        virtual void onEvent(const EntityChangedEvent& event) override
        {
            changedEntities.push(event);
        }

        virtual void execute() override
        {
            processClear();
            processVisibility();
            processAdd();
            processRemove();
            processChanged();

            for (auto ent : layoutUpdate)
            {
                if (not ent->has<PositionComponent>())
                    continue;

                if (ent->has<HorizontalLayout>())
                {
                    auto view = ent->get<HorizontalLayout>();

                    recalculateChildrenPos(ent, view);

                    updateVisibility(ent, view, view->visible);
                }

                if (ent->has<VerticalLayout>())
                {
                    auto view = ent->get<VerticalLayout>();

                    recalculateChildrenPos(ent, view);

                    updateVisibility(ent, view, view->visible);
                }
            }

            layoutUpdate.clear();
        }

        void processClear()
        {
            while (not clearQueue.empty())
            {
                const auto& event = clearQueue.front();
                auto ent = ecsRef->getEntity(event.id);

                if (ent)
                {
                    if (event.orientation == LayoutOrientation::Horizontal and ent->has<HorizontalLayout>())
                        clear(ent->get<HorizontalLayout>());

                    if (event.orientation == LayoutOrientation::Vertical and ent->has<VerticalLayout>())
                        clear(ent->get<VerticalLayout>());

                    layoutUpdate.insert(ent);
                }

                clearQueue.pop();
            }
        }

        void processVisibility()
        {
            while (not visibilityQueue.empty())
            {
                const auto& event = visibilityQueue.front();
                auto ent = ecsRef->getEntity(event.id);

                if (ent)
                {
                    if (ent->has<HorizontalLayout>())
                        ent->get<HorizontalLayout>()->visible = event.visible;

                    if (ent->has<VerticalLayout>())
                        ent->get<VerticalLayout>()->visible = event.visible;

                    if (ent->has<PositionComponent>())
                        ent->get<PositionComponent>()->visible = event.visible;

                    layoutUpdate.insert(ent);
                }

                visibilityQueue.pop();
            }
        }

        void processAdd()
        {
            while (not addQueue.empty())
            {
                const auto& event = addQueue.front();
                auto ent = ecsRef->getEntity(event.id);

                if (ent and (ent->has<HorizontalLayout>() or ent->has<VerticalLayout>()))
                {
                    entitiesInLayout.insert(event.id);
                    addEntity(ent, event.ui, event.orientation);

                    layoutUpdate.insert(ent);
                }

                addQueue.pop();
            }
        }

        void processRemove()
        {
            while (not removeQueue.empty())
            {
                const auto& event = removeQueue.front();
                auto ent = ecsRef->getEntity(event.id);

                if (ent)
                {
                    if (event.orientation == LayoutOrientation::Horizontal and ent->has<HorizontalLayout>())
                    {
                        removeEntity(ent->get<HorizontalLayout>(), event.index);
                    }

                    if (event.orientation == LayoutOrientation::Vertical and ent->has<VerticalLayout>())
                    {
                        removeEntity(ent->get<VerticalLayout>(), event.index);
                    }

                    entitiesInLayout.erase(event.id);

                    layoutUpdate.insert(ent);
                }

                removeQueue.pop();
            }
        }

        void processChanged()
        {
            while (not changedEntities.empty())
            {
                const auto& event = changedEntities.front();
                auto ent = ecsRef->getEntity(event.id);

                if (not ent)
                {
                    changedEntities.pop();
                    continue;
                }

                if (ent->has<HorizontalLayout>() or ent->has<VerticalLayout>())
                {
                    layoutUpdate.insert(ent);
                    changedEntities.pop();
                    continue;
                }

                // If entity is not in a layout anymore we can skip the heavy lookup in layouts
                if (not entitiesInLayout.count(ent->id))
                {
                    changedEntities.pop();
                    continue;
                }

                bool found = false;

                // Todo maybe add a flag to all the entity put in a layout so we can just check for the flag presence and get rid of this
                // An entity should not be in multple layouts at the same time
                for (auto v : view<HorizontalLayout>())
                {
                    const auto& it = std::find_if(v->entities.begin(), v->entities.end(), [ent](const EntityRef& ref) { return ref.id == ent->id; });

                    if (it != v->entities.end())
                    {
                        layoutUpdate.insert(ecsRef->getEntity(v->id));
                        changedEntities.pop();

                        found = true;
                        break;
                    }
                }

                if (found)
                    continue;

                for (auto v : view<VerticalLayout>())
                {
                    const auto& it = std::find_if(v->entities.begin(), v->entities.end(), [ent](const EntityRef& ref) { return ref.id == ent->id; });

                    if (it != v->entities.end())
                    {
                        layoutUpdate.insert(ecsRef->getEntity(v->id));
                        changedEntities.pop();

                        found = true;
                        break;
                    }
                }

                if (found)
                    continue;

                changedEntities.pop();
            }
        }

        template <typename Layout>
        void recalculateChildrenPos(EntityRef viewEnt, Layout view)
        {
            auto orientation = view->orientation;

            auto viewUi = viewEnt->template get<PositionComponent>();

            // This snippet handles the layout of entities within a parent view when neither `fitToAxis` nor `spaced` is enabled.
            // It aligns entities along the primary axis (horizontal or vertical) by setting their anchor relative to the previous entity's anchor, with a specified spacing.
            // The secondary axis is aligned with the parent view's anchor.
            // Finally, the parent view's size along the primary axis is updated to encompass all child entities.
            if (not view->fitToAxis and not view->spaced)
            {
                float currentX = viewUi->x;
                float currentY = viewUi->y;

                for (auto& ent : view->entities)
                {
                    if (not ent->template has<PositionComponent>())
                    {
                        LOG_ERROR("Layout", "Entity " << ent.id << " must have a PositionComponent!");
                        continue;
                    }

                    auto pos = ent->template get<PositionComponent>();

                    // Skip invisible components
                    if (not pos->visible)
                        continue;

                    if (orientation == LayoutOrientation::Horizontal)
                    {
                        pos->setX(currentX);
                        pos->setY(viewUi->y); // Align with the top of the parent layout
                        currentX += pos->width + view->spacing; // Move to the next position
                    }
                    else if (orientation == LayoutOrientation::Vertical)
                    {
                        pos->setX(viewUi->x); // Align with the left of the parent layout
                        pos->setY(currentY);
                        currentY += pos->height + view->spacing; // Move to the next position
                    }
                }

                // Todo the view is not properly placed if it is not sized to content !
                if (view->sizedToContent)
                {
                    if (orientation == LayoutOrientation::Horizontal)
                    {
                        viewUi->setWidth(currentX - viewUi->x);
                    }
                    else if (orientation == LayoutOrientation::Vertical)
                    {
                        viewUi->setHeight(currentY - viewUi->y);
                    }
                }

                return;
            }

            float start, axis, axisSize;

            if (orientation == LayoutOrientation::Horizontal)
            {
                start = viewUi->x;
                axisSize = viewUi->width;
            }
            else if (orientation == LayoutOrientation::Vertical)
            {
                start = viewUi->y;
                axisSize = viewUi->height;
            }
            else
            {
                LOG_ERROR("Layout", "Invalid orientation for layout: " << static_cast<int>(orientation));
                return;
            }

            float currentX = viewUi->x, currentY = viewUi->y, maxVal = 0.0f;
            size_t nbCurrentElement = 0;

            for (size_t i = 0; i < view->entities.size(); ++i)
            {
                auto ent = view->entities[i];

                if (not ent->template has<PositionComponent>())
                {
                    LOG_ERROR("Layout", "Entity " << ent.id << " must have a PositionComponent!");
                    continue;
                }

                auto pos = ent->template get<PositionComponent>();

                // Skip invisible components
                if (not pos->visible)
                    continue;

                if (orientation == LayoutOrientation::Horizontal)
                {
                    axis = pos->height;
                }
                else if (orientation == LayoutOrientation::Vertical)
                {
                    axis = pos->width;
                }

                if (axis > maxVal)
                    maxVal = axis;
            }

            auto totalVal = 0.0f;

            float currentSize = 0.0f;
            size_t firstElementIndex = 0;

            const float maxPos = start + axisSize;

            bool lastElementWasOversized = false;

            for (size_t i = 0; i < view->entities.size(); i++)
            {
                auto ent = view->entities[i];

                if (not ent->template has<PositionComponent>())
                {
                    LOG_ERROR("Layout", "Entity " << ent.id << " must have a PositionComponent!");
                    continue;
                }

                auto pos = ent->template get<PositionComponent>();

                // Skip invisible components
                if (not pos->visible)
                    continue;

                float posSecondAxis;
                float *boundAxis, *otherAxis;

                if (orientation == LayoutOrientation::Horizontal)
                {
                    posSecondAxis = pos->width;

                    boundAxis = &currentY;
                    otherAxis = &currentX;
                }
                else if (orientation == LayoutOrientation::Vertical)
                {
                    posSecondAxis = pos->height;

                    boundAxis = &currentX;
                    otherAxis = &currentY;
                }

                if (nbCurrentElement == 0)
                {
                    totalVal += maxVal + view->spacing;

                    if (posSecondAxis >= axisSize)
                    {
                        lastElementWasOversized = true;

                        pos->setX(currentX);
                        pos->setY(currentY);

                        *boundAxis += (orientation == LayoutOrientation::Vertical ? pos->width : pos->height) + view->spacing;
                        firstElementIndex = i + 1;
                        continue;
                    }
                }

                lastElementWasOversized = false;

                if (view->fitToAxis and (*otherAxis + posSecondAxis > maxPos))
                {
                    *otherAxis = start;
                    *boundAxis += maxVal + view->spacing;

                    if (view->spaced)
                    {
                        auto elemSpacing = nbCurrentElement > 1 ? (axisSize - currentSize) / (nbCurrentElement - 1) : (axisSize - currentSize);
                        auto currentSpacedStart = start;

                        for (size_t j = 0; j < nbCurrentElement; j++)
                        {
                            auto columnEnt = view->entities[firstElementIndex + j];
                            auto columnPos = columnEnt->template get<PositionComponent>();

                            if (not columnPos->visible)
                                continue;

                            if (orientation == LayoutOrientation::Horizontal)
                            {
                                columnPos->setX(currentSpacedStart);
                                currentSpacedStart += columnPos->width + elemSpacing;
                            }
                            else if (orientation == LayoutOrientation::Vertical)
                            {
                                columnPos->setY(currentSpacedStart);
                                currentSpacedStart += columnPos->height + elemSpacing;
                            }
                        }

                        currentSize = 0;
                        firstElementIndex = i;
                    }

                    i--;
                    nbCurrentElement = 0;
                    continue;
                }

                if (view->spaced)
                {
                    currentSize += posSecondAxis;
                }
                else
                {
                    if (orientation == LayoutOrientation::Horizontal)
                    {
                        pos->setX(*otherAxis);
                    }
                    else if (orientation == LayoutOrientation::Vertical)
                    {
                        pos->setY(*otherAxis);
                    }
                }

                if (orientation == LayoutOrientation::Horizontal)
                {
                    pos->setY(*boundAxis);
                }
                else if (orientation == LayoutOrientation::Vertical)
                {
                    pos->setX(*boundAxis);
                }

                *otherAxis += posSecondAxis + view->spacing;
                nbCurrentElement++;
            }

            if (nbCurrentElement == 0 and not lastElementWasOversized)
            {
                totalVal += maxVal + view->spacing;
            }

            if (orientation == LayoutOrientation::Horizontal)
            {
                viewUi->setHeight(totalVal);
            }
            else if (orientation == LayoutOrientation::Vertical)
            {
                viewUi->setWidth(totalVal);
            }

            if (view->spaced and (firstElementIndex < view->entities.size()))
            {
                // The +1 ensures that spacing is applied to all gaps between elements,
                // including the space before the first and after the last.
                auto elemSpacing = (axisSize - currentSize) / (nbCurrentElement + 1);
                auto currentSpacedStart = start;

                if (currentSize > axisSize)
                    elemSpacing = view->spacing;
                else
                    currentSpacedStart += elemSpacing;

                for (size_t j = 0; j < nbCurrentElement; j++)
                {
                    auto columnEnt = view->entities[firstElementIndex + j];
                    auto columnPos = columnEnt->template get<PositionComponent>();

                    if (not columnPos->visible)
                        continue;

                    if (orientation == LayoutOrientation::Horizontal)
                    {
                        columnPos->setX(currentSpacedStart);
                        currentSpacedStart += columnPos->width + elemSpacing;
                    }
                    else if (orientation == LayoutOrientation::Vertical)
                    {
                        columnPos->setY(currentSpacedStart);
                        currentSpacedStart += columnPos->height + elemSpacing;
                    }
                }

                currentSize = 0;
            }
        }

        void addEntity(EntityRef viewEnt, _unique_id ui, LayoutOrientation orientation);

        template <typename Layout>
        void removeEntity(Layout view, _unique_id index)
        {
            auto it = std::find_if(view->entities.begin(), view->entities.end(), [index](const EntityRef& entity) { return entity.id == index; });

            if (it != view->entities.end())
            {
                view->entities.erase(it);
                ecsRef->removeEntity(index);
            }
        }

        template <typename Layout>
        void updateVisibility(EntityRef viewEnt, Layout view, bool visible)
        {
            view->visible = visible;

            auto& entities = view->entities;

            for (auto& ui : entities)
            {
                if (ui->template has<PositionComponent>())
                {
                    auto pos = ui->template get<PositionComponent>();
                    bool isCompVisible = false;

                    if (visible)
                    {
                        float childTop    = pos->y;
                        float childBottom = pos->y + pos->height;
                        float childLeft   = pos->x;
                        float childRight  = pos->x + pos->width;

                        if (inBound(viewEnt, childLeft, childTop) or inBound(viewEnt, childLeft, childBottom) or inBound(viewEnt, childRight, childTop) or inBound(viewEnt, childRight, childBottom))
                        {
                            isCompVisible = true;
                        }
                    }

                    pos->setObservable(isCompVisible);
                }
            }
        }

        template <typename Layout>
        void clear(Layout view)
        {
            for (auto ent : view->entities)
            {
                entitiesInLayout.erase(ent.id);
                ecsRef->removeEntity(ent);
            }

            view->entities.clear();
        }

        std::queue<AddLayoutElementEvent> addQueue;
        std::queue<RemoveLayoutElementEvent> removeQueue;
        std::queue<ClearLayoutEvent> clearQueue;
        std::queue<UpdateLayoutVisibility> visibilityQueue;
        std::queue<EntityChangedEvent> changedEntities;

        std::set<EntityRef> layoutUpdate;

        std::set<_unique_id> entitiesInLayout;
    };

    template <typename Type>
    CompList<PositionComponent, UiAnchor, HorizontalLayout> makeHorizontalLayout(Type *ecs, float x, float y, float width, float height)
    {
        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        auto view = ecs->template attach<HorizontalLayout>(entity);

        ui->setX(x);
        ui->setY(y);
        ui->setWidth(width);
        ui->setHeight(height);

        return {entity, ui, anchor, view};
    }

    template <typename Type>
    CompList<PositionComponent, UiAnchor, VerticalLayout> makeVerticalLayout(Type *ecs, float x, float y, float width, float height)
    {
        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        auto view = ecs->template attach<VerticalLayout>(entity);

        ui->setX(x);
        ui->setY(y);
        ui->setWidth(width);
        ui->setHeight(height);

        return {entity, ui, anchor, view};
    }
}