#include "stdafx.h"

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

            if (vLayout->scrollable)
                entity->world()->attach<MouseWheelComponent>(entity, StandardEvent{"layoutScroll", "id", entity->id});
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

            if (hLayout->scrollable)
                entity->world()->attach<MouseWheelComponent>(entity, StandardEvent{"layoutScroll", "id", entity->id});
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

        if (ent->has<VerticalLayout>())
        {
            auto comp = ent->get<VerticalLayout>();
            offset = &comp->yOffset;
            scrollSpeed = comp->scrollSpeed;
        }
        else if (ent->has<HorizontalLayout>())
        {
            auto comp = ent->get<HorizontalLayout>();
            offset = &comp->xOffset;
            scrollSpeed = comp->scrollSpeed;
        }
        else
        {
            LOG_ERROR(DOM, "Error entity: " << id << " is not a layout!");
            return;
        }

        *offset -= event.values.at("y").get<int>() * scrollSpeed;

        ecsRef->sendEvent(EntityChangedEvent{id});
    };

    void LayoutSystem::execute()
    {
        for (auto ent : layoutUpdate)
        {
            if (not ent->has<PositionComponent>())
                continue;

            if (ent->has<HorizontalLayout>())
            {
                auto view = ent->get<HorizontalLayout>();
                updateLayout(ent, view);
            }

            if (ent->has<VerticalLayout>())
            {
                auto view = ent->get<VerticalLayout>();
                updateLayout(ent, view);
            }
        }

        layoutUpdate.clear();
    }

    void LayoutSystem::updateLayout(EntityRef viewEnt, BaseLayout* view)
    {
        recalculateChildrenPos(viewEnt, view);
        updateVisibility(viewEnt, view);
    }

    void LayoutSystem::onProcessEvent(const UpdateLayoutScrollable& event)
    {
        auto ent = ecsRef->getEntity(event.id);

        if (ent)
        {
            if (event.orientation == LayoutOrientation::Horizontal and ent->has<HorizontalLayout>())
            {
                processScrollHelper(ent, ent->get<HorizontalLayout>(), event.scrollable);
            }

            if (event.orientation == LayoutOrientation::Vertical and ent->has<VerticalLayout>())
            {
                processScrollHelper(ent, ent->get<VerticalLayout>(), event.scrollable);
            }

            layoutUpdate.insert(ent);
        }
    }

    void LayoutSystem::processScrollHelper(Entity* entity, BaseLayout* view, bool scrollable)
    {
        if (scrollable)
        {
            ecsRef->attach<MouseWheelComponent>(entity, StandardEvent{"layoutScroll", "id", entity->id});

            for (auto& ent : view->entities)
            {
                ecsRef->attach<ClippedTo>(ent, entity->id);
            }
        }
        else
        {
            ecsRef->detach<MouseWheelComponent>(entity);

            if (not entity->has<ClippedTo>())
            {
                for (auto& ent : view->entities)
                {
                    if (ent->has<ClippedTo>())
                        ecsRef->detach<ClippedTo>(ent);
                }
            }

            if (not view->horizontalScrollBar.empty() and view->horizontalScrollBar.has<PositionComponent>())
            {
                auto sbPos = view->horizontalScrollBar.get<PositionComponent>();

                sbPos->setVisibility(false);
            }

            if (not view->verticalScrollBar.empty() and view->verticalScrollBar.has<PositionComponent>())
            {
                auto sbPos = view->verticalScrollBar.get<PositionComponent>();

                sbPos->setVisibility(false);
            }
        }
    }

    void LayoutSystem::onProcessEvent(const ClearLayoutEvent& event)
    {
        clear(event.entityIds);
    }

    void LayoutSystem::onProcessEvent(const AddLayoutElementEvent& event)
    {
        auto ent = ecsRef->getEntity(event.id);

        if (ent and (ent->has<HorizontalLayout>() or ent->has<VerticalLayout>()))
        {
            entitiesInLayout.insert(event.id);
            addEntity(ent, event.ui, event.orientation);

            layoutUpdate.insert(ent);
        }
    }

    void LayoutSystem::onProcessEvent(const InsertLayoutElementEvent& event)
    {
        auto ent = ecsRef->getEntity(event.id);

        if (ent and (ent->has<HorizontalLayout>() or ent->has<VerticalLayout>()))
        {
            entitiesInLayout.insert(event.id);
            addEntity(ent, event.ui, event.orientation, event.index);

            layoutUpdate.insert(ent);
        }
    }

    void LayoutSystem::onProcessEvent(const RemoveLayoutElementEvent& event)
    {
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
    }

    void LayoutSystem::onProcessEvent(const RemoveLayoutElementAtEvent& event)
    {
        auto ent = ecsRef->getEntity(event.id);

        if (ent)
        {
            if (event.orientation == LayoutOrientation::Horizontal and ent->has<HorizontalLayout>())
            {
                removeEntityAt(ent->get<HorizontalLayout>(), event.index);
            }

            if (event.orientation == LayoutOrientation::Vertical and ent->has<VerticalLayout>())
            {
                removeEntityAt(ent->get<VerticalLayout>(), event.index);
            }

            entitiesInLayout.erase(event.id);

            layoutUpdate.insert(ent);
        }
    }

    void LayoutSystem::onProcessEvent(const EntityChangedEvent& event)
    {
        auto ent = ecsRef->getEntity(event.id);

        if (not ent)
        {
            return;
        }

        if (ent->has<HorizontalLayout>() or ent->has<VerticalLayout>())
        {
            layoutUpdate.insert(ent);
            return;
        }

        // If entity is not in a layout anymore we can skip the heavy lookup in layouts
        if (not entitiesInLayout.count(ent->id))
        {
            return;
        }

        // Todo maybe add a flag to all the entity put in a layout so we can just check for the flag presence and get rid of this
        // An entity should not be in multple layouts at the same time
        for (auto v : view<HorizontalLayout>())
        {
            const auto& it = std::find_if(v->entities.begin(), v->entities.end(), [ent](const EntityRef& ref) { return ref.id == ent->id; });

            if (it != v->entities.end())
            {
                layoutUpdate.insert(ecsRef->getEntity(v->id));
                return;
            }
        }

        for (auto v : view<VerticalLayout>())
        {
            const auto& it = std::find_if(v->entities.begin(), v->entities.end(), [ent](const EntityRef& ref) { return ref.id == ent->id; });

            if (it != v->entities.end())
            {
                layoutUpdate.insert(ecsRef->getEntity(v->id));
                return;
            }
        }
    }

    void LayoutSystem::recalculateChildrenPos(EntityRef viewEnt, BaseLayout* view)
    {
        auto childrenAdded = view->childrenAdded;
        view->childrenAdded = false;

        adjustOffsets(viewEnt, view, childrenAdded);
        updateScrollBars(viewEnt, view);

        if (not view->fitToAxis and not view->spaced)
        {
            layoutWithoutSpacing(viewEnt, view);
            return;
        }

        layoutWithSpacing(viewEnt, view);
    }

    void LayoutSystem::adjustOffsets(EntityRef viewEnt, BaseLayout* view, bool childrenAdded)
    {
        auto viewUi = viewEnt->get<PositionComponent>();

        // If a child was added, while stick to end == true, the offset was set 'out of bound' because the size was not adjusted yet !
        if (not childrenAdded)
        {
            view->xOffset = std::max(0.0f, std::min(view->xOffset, view->contentWidth  - viewUi->width));
            view->yOffset = std::max(0.0f, std::min(view->yOffset, view->contentHeight - viewUi->height));
        }
    }

    void LayoutSystem::updateScrollBars(EntityRef viewEnt, BaseLayout* view)
    {
        auto viewUi = viewEnt->get<PositionComponent>();

        if (not view->horizontalScrollBar.empty() and view->horizontalScrollBar.has<PositionComponent>())
        {
            auto sbPos = view->horizontalScrollBar.get<PositionComponent>();
            updateHorizontalScrollBar(viewUi, view, sbPos);
        }

        if (not view->verticalScrollBar.empty() and view->verticalScrollBar.has<PositionComponent>())
        {
            auto sbPos = view->verticalScrollBar.get<PositionComponent>();
            updateVerticalScrollBar(viewUi, view, sbPos);
        }
    }

    void LayoutSystem::updateHorizontalScrollBar(PositionComponent* viewUi, BaseLayout* view, PositionComponent* sbPos)
    {
        if (areNotAlmostEqual(viewUi->width, view->contentWidth) and areNotAlmostEqual(view->contentWidth, 0))
        {
            float thumbWidth = (viewUi->width / view->contentWidth) * viewUi->width;
            sbPos->setX(viewUi->x + (view->xOffset / (view->contentWidth - viewUi->width)) * (viewUi->width - thumbWidth));
            sbPos->setWidth(thumbWidth);
            sbPos->setVisibility(true);
        }
        else
        {
            sbPos->setVisibility(false);
        }
    }

    void LayoutSystem::updateVerticalScrollBar(PositionComponent* viewUi, BaseLayout* view, PositionComponent* sbPos)
    {
        if (areNotAlmostEqual(viewUi->height, view->contentHeight) and areNotAlmostEqual(view->contentHeight, 0))
        {
            float thumbHeight = (viewUi->height / view->contentHeight) * viewUi->height;
            sbPos->setY(viewUi->y + (view->yOffset / (view->contentHeight - viewUi->height)) * (viewUi->height - thumbHeight));
            sbPos->setHeight(thumbHeight);
            sbPos->setVisibility(true);
        }
        else
        {
            sbPos->setVisibility(false);
        }
    }

    // This snippet handles the layout of entities within a parent view when neither `fitToAxis` nor `spaced` is enabled.
    // It aligns entities along the primary axis (horizontal or vertical) by setting their anchor relative to the previous entity's anchor, with a specified spacing.
    // The secondary axis is aligned with the parent view's anchor.
    // Finally, the parent view's size along the primary axis is updated to encompass all child entities.
    void LayoutSystem::layoutWithoutSpacing(EntityRef viewEnt, BaseLayout* view)
    {
        auto viewUi = viewEnt->template get<PositionComponent>();

        float currentX = viewUi->x - view->xOffset;
        float currentY = viewUi->y - view->yOffset;

        view->contentWidth = 0.0f;
        view->contentHeight = 0.0f;

        auto orientation = view->orientation;

        for (auto& ent : view->entities)
        {
            if (not ent->has<PositionComponent>())
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
                pos->setX(currentX + view->contentWidth);
                pos->setY(viewUi->y); // Align with the top of the parent layout
                view->contentWidth += pos->width + view->spacing; // Move to the next position
                view->contentHeight = std::max(view->contentHeight, pos->height);
            }
            else if (orientation == LayoutOrientation::Vertical)
            {
                pos->setX(viewUi->x); // Align with the left of the parent layout
                pos->setY(currentY + view->contentHeight);
                view->contentHeight += pos->height + view->spacing; // Move to the next position
                view->contentWidth = std::max(view->contentWidth, pos->width);
            }
        }

        // Todo the view is not properly placed if it is not sized to content !
        if (not view->scrollable)
        {
            bool hConstrained = false;
            bool wConstrained = false;

            if (viewEnt->has<UiAnchor>())
            {
                auto anchor = viewEnt->get<UiAnchor>();

                wConstrained = anchor->hasWidthConstrain or (anchor->hasRightAnchor and anchor->hasLeftAnchor);
                hConstrained = anchor->hasHeightConstrain or (anchor->hasTopAnchor and anchor->hasBottomAnchor);
            }

            if (not wConstrained)
                viewUi->setWidth(view->contentWidth);

            if (not hConstrained)
                viewUi->setHeight(view->contentHeight);
        }
    }

    void LayoutSystem::layoutWithSpacing(EntityRef viewEnt, BaseLayout* view)
    {
        auto viewUi = viewEnt->template get<PositionComponent>();

        float start, axis, axisSize;

        auto orientation = view->orientation;

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

        float currentX = viewUi->x - view->xOffset;
        float currentY = viewUi->y - view->yOffset;
        float maxVal = 0.0f;
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

    void LayoutSystem::addEntity(EntityRef viewEnt, _unique_id ui, LayoutOrientation orientation, int index)
    {
        if (not viewEnt->has<PositionComponent>())
        {
            LOG_ERROR(DOM, "Entity " << viewEnt.id << " must have a PositionComponent!");
            return;
        }

        auto ent = ecsRef->getEntity(ui);

        if (not ent or not ent->has<PositionComponent>())
        {
            LOG_ERROR(DOM, "Entity " << ui << " must have a PositionComponent!");
            return;
        }

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

            if (index < 0)
            {
                index = static_cast<int>(view->entities.size()) + index + 1;
            }

            if (index < 0 or index > static_cast<int>(view->entities.size()))
            {
                LOG_ERROR(DOM, "Index " << index << " is out of bounds for layout with id: " << viewEnt.id);
                return;
            }

            LOG_INFO(DOM, "Add entity " << ent->id << " to layout with id: " << viewEnt.id << " at index: " << index);

            if (view->scrollable)
            {
                ecsRef->attach<ClippedTo>(ent, view->id);
            }

            if (viewEnt->has<ClippedTo>())
            {
                ecsRef->attach<ClippedTo>(ent, viewEnt->get<ClippedTo>()->clipperId);
            }

            view->entities.insert(view->entities.begin() + index, ent);

            // view->entities.push_back(ent);

            // Stick to end logic for horizontal layout
            if (view->stickToEnd)
            {
                auto viewUi = viewEnt->get<PositionComponent>();
                auto entUi = ent->get<PositionComponent>();

                // If another child was added during the same execute pass, we just need to adjust the offset
                if (not view->childrenAdded)
                    view->xOffset = std::max(0.0f, view->contentWidth - viewUi->width + entUi->width);
                else
                    view->xOffset += entUi->width;
            }

            view->childrenAdded = true;
        }
        else if (orientation == LayoutOrientation::Vertical)
        {
            auto view = viewEnt->get<VerticalLayout>();

            if (index < 0)
            {
                index = static_cast<int>(view->entities.size()) + index + 1;
            }

            if (index < 0 or index > static_cast<int>(view->entities.size()))
            {
                LOG_ERROR(DOM, "Index " << index << " is out of bounds for layout with id: " << viewEnt.id);
                return;
            }

            LOG_INFO(DOM, "Add entity " << ent->id << " to layout with id: " << viewEnt.id << " at index: " << index);

            if (view->scrollable)
            {
                ecsRef->attach<ClippedTo>(ent, view->id);
            }

            if (viewEnt->has<ClippedTo>())
            {
                ecsRef->attach<ClippedTo>(ent, viewEnt->get<ClippedTo>()->clipperId);
            }

            // view->entities.push_back(ent);
            view->entities.insert(view->entities.begin() + index, ent);

            // Stick to end logic for vertical layout
            if (view->stickToEnd)
            {
                auto viewUi = viewEnt->get<PositionComponent>();
                auto entUi = ent->get<PositionComponent>();

                // If another child was added during the same execute pass, we just need to adjust the offset
                if (not view->childrenAdded)
                    view->yOffset = std::max(0.0f, view->contentHeight - viewUi->height + entUi->height);
                else
                    view->yOffset += entUi->height;
            }

            view->childrenAdded = true;
        }
        else
        {
            LOG_ERROR(DOM, "Invalid orientation for layout: " << static_cast<int>(orientation));
            return;
        }
    }

    void LayoutSystem::removeEntity(BaseLayout* view, _unique_id index)
    {
        auto it = std::find_if(view->entities.begin(), view->entities.end(), [index](const EntityRef& entity) { return entity.id == index; });

        if (it != view->entities.end())
        {
            ecsRef->sendEvent(ClearParentingEvent{index, view->id});

            view->entities.erase(it);
            ecsRef->removeEntity(index);
        }
    }

    void LayoutSystem::removeEntityAt(BaseLayout* view, int index)
    {
        if (index < 0)
        {
            index = static_cast<int>(view->entities.size()) + index + 1;
        }

        if (index < 0 or index >= static_cast<int>(view->entities.size()))
        {
            LOG_ERROR(DOM, "Index " << index << " is out of bounds for layout with id: " << view->id);
            return;
        }

        _unique_id indexId = view->entities[index].id;

        ecsRef->sendEvent(ClearParentingEvent{indexId, view->id});

        view->entities.erase(view->entities.begin() + index);
        ecsRef->removeEntity(indexId);
    }

    void LayoutSystem::updateVisibility(EntityRef viewEnt, BaseLayout* view)
    {
        auto& entities = view->entities;

        // Parent clipping bounds
        auto parentPos = viewEnt->template get<PositionComponent>();
        auto renderable = parentPos->isRenderable();

        float parentTop    = parentPos->y;
        float parentBottom = parentPos->y + parentPos->height;
        float parentLeft   = parentPos->x;
        float parentRight  = parentPos->x + parentPos->width;

        for (auto& ui : entities)
        {
            if (ui->template has<PositionComponent>())
            {
                auto pos = ui->template get<PositionComponent>();
                bool isCompVisible = false;

                if (renderable)
                {
                    float childTop    = pos->y;
                    float childBottom = pos->y + pos->height;
                    float childLeft   = pos->x;
                    float childRight  = pos->x + pos->width;

                    if (childRight > parentLeft and childLeft < parentRight and childBottom > parentTop and childTop < parentBottom)
                    {
                        isCompVisible = true;
                    }
                }

                pos->setObservable(isCompVisible);
            }
        }
    }

    void LayoutSystem::clear(const std::vector<_unique_id>& entityIds)
    {
        for (auto id : entityIds)
        {
            entitiesInLayout.erase(id);
            ecsRef->removeEntity(id);
        }
    }

}