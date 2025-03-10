#include "sizer.h"

namespace pg
{
    void HorizontalLayoutSystem::addEntity(EntityRef viewEnt, _unique_id ui)
    {
        if (not viewEnt->has<PositionComponent>() or not viewEnt->has<UiAnchor>())
        {
            LOG_ERROR("ListViewSystem", "Entity " << viewEnt.id << " must have a PositionComponent and UiAnchor!");
            return;
        }

        auto ent = ecsRef->getEntity(ui);

        if (not ent or not ent->has<PositionComponent>() or not ent->has<UiAnchor>())
        {
            LOG_ERROR("ListViewSystem", "Entity " << ui << " must have a PositionComponent and UiAnchor!");
            return;
        }

        auto view = viewEnt->get<HorizontalLayout>();
        // auto viewUi = viewEnt->get<PositionComponent>();
        // auto viewAnchor = viewEnt->get<UiAnchor>();

        auto uiAnchor = ent->get<UiAnchor>();

        // Z + 1 so the initial z of the list is for the background
        uiAnchor->setZConstrain(PosConstrain{viewEnt.id, AnchorType::Z, PosOpType::Add, 1});

        view->entities.push_back(ent);

        recalculateChildrenPos(viewEnt);

        updateVisibility(viewEnt, view->visible);
    }

    void HorizontalLayoutSystem::recalculateChildrenPos(EntityRef viewEnt)
    {
        auto view = viewEnt->get<HorizontalLayout>();
        auto viewUi = viewEnt->get<PositionComponent>();

        float currentX = viewUi->x, currentY = viewUi->y, maxHeight = 0.0f;
        size_t nbElementOnCurrentRow = 0;

        float currentRowWidth = 0.0f;
        size_t firstRowElementIndex = 0;

        const float maxWidthPos = viewUi->x + viewUi->width;

        for (size_t i = 0; i < view->entities.size(); i++)
        {
            auto ent = view->entities[i];

            if (not ent->has<PositionComponent>())
            {
                LOG_ERROR("HorizontalLayoutSystem", "Entity " << ent.id << " must have a PositionComponent!");
                continue;
            }

            auto pos = ent->get<PositionComponent>();

            if (nbElementOnCurrentRow == 0 and pos->width >= viewUi->width)
            {
                pos->setX(currentX);
                pos->setY(currentY);

                currentY += pos->height + view->spacing;

                firstRowElementIndex = i + 1;

                continue;
            }

            if (view->fitToWidth and (currentX + pos->width > maxWidthPos))
            {
                currentX = viewUi->x;
                currentY += maxHeight + view->spacing;

                if (view->spacedInWidth)
                {
                    auto rowSpacing = nbElementOnCurrentRow > 1 ? (viewUi->width - currentRowWidth) / (nbElementOnCurrentRow - 1) : (viewUi->width - currentRowWidth);
                    auto currentRowX = viewUi->x;

                    for (size_t j = 0; j < nbElementOnCurrentRow; j++)
                    {
                        auto rowEnt = view->entities[firstRowElementIndex + j];

                        auto rowPos = rowEnt->get<PositionComponent>();

                        rowPos->setX(currentRowX);

                        currentRowX += rowPos->width + rowSpacing;
                    }

                    currentRowWidth = 0;

                    firstRowElementIndex = i;
                }

                // We decrement the counter to stay on the current entity
                i--;
                nbElementOnCurrentRow = 0;
                continue;
            }

            if (view->spacedInWidth)
            {
                // If we space the entities horizontally, we can't update the x until the whole row has been processed
                currentRowWidth += pos->width;
            }
            else
            {
                pos->setX(currentX);
            }

            pos->setY(currentY);

            currentX += pos->width + view->spacing;
            maxHeight = std::max(maxHeight, pos->height);
            nbElementOnCurrentRow++;
        }

        if (view->spacedInWidth and (firstRowElementIndex < view->entities.size()))
        {
            auto rowSpacing = (viewUi->width - currentRowWidth) / (nbElementOnCurrentRow + 1);
            auto currentRowX = viewUi->x;

            if (currentRowWidth > viewUi->width)
                rowSpacing = view->spacing;
            else
                currentRowX += rowSpacing;

            for (size_t j = 0; j < nbElementOnCurrentRow; j++)
            {
                auto rowEnt = view->entities[firstRowElementIndex + j];

                auto rowPos = rowEnt->get<PositionComponent>();

                rowPos->setX(currentRowX);

                currentRowX += rowPos->width + rowSpacing;
            }

            currentRowWidth = 0;
        }
    }

    void HorizontalLayoutSystem::updateVisibility(EntityRef viewEnt, bool visible)
    {
        auto& entities = viewEnt->get<HorizontalLayout>()->entities;

        for (auto& ui : entities)
        {
            if (ui->has<PositionComponent>())
            {
                auto pos = ui->get<PositionComponent>();
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

                pos->setVisibility(isCompVisible);
            }            
        }
    }

    void HorizontalLayoutSystem::clear(CompRef<HorizontalLayout> view)
    {
        for (auto ent : view->entities)
        {
            ecsRef->removeEntity(ent);
        }

        view->entities.clear();
    }
}