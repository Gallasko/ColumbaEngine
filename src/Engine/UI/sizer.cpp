#include "sizer.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Layout";
    }


    void HorizontalLayoutSystem::init()
    {
        auto group = registerGroup<PositionComponent, HorizontalLayout>();

        group->addOnGroup([](EntityRef entity) {
            LOG_MILE(DOM, "Add entity " << entity->id << " to ui - hLayout group !");

            auto hLayout = entity->get<HorizontalLayout>();
            auto position = entity->get<PositionComponent>();

            hLayout->visible = position->visible;
        });
    }

    void HorizontalLayoutSystem::addEntity(EntityRef viewEnt, _unique_id ui)
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

        auto view = viewEnt->get<HorizontalLayout>();
        auto uiAnchor = ent->get<UiAnchor>();

        // Z + 1 so the initial z of the list is for the background
        uiAnchor->setZConstrain(PosConstrain{viewEnt.id, AnchorType::Z, PosOpType::Add, 1});

        view->entities.push_back(ent);
    }

    void HorizontalLayoutSystem::removeEntity(EntityRef viewEnt, _unique_id index)
    {
        LOG_INFO(DOM, "Removing Entity " << index);
        auto view = viewEnt->get<HorizontalLayout>();

        auto it = std::find_if(view->entities.begin(), view->entities.end(), [index](const EntityRef& entity) { return entity.id == index; });

        if (it != view->entities.end())
        {
            view->entities.erase(it);
            ecsRef->removeEntity(index);
        }
    }

    void HorizontalLayoutSystem::recalculateChildrenPos(EntityRef viewEnt)
    {
        auto view = viewEnt->get<HorizontalLayout>();
        auto viewUi = viewEnt->get<PositionComponent>();

        // This snippet handles the horizontal layout of entities within a parent view when neither `fitToWidth` nor `spacedInWidth` is enabled.
        // It aligns entities horizontally by setting their left anchor relative to the previous entity's right anchor, with a specified spacing.
        // The top anchor of each entity is aligned with the top anchor of the parent view.
        // Finally, the parent view's right anchor is updated to encompass all child entities.
        if (not view->fitToWidth and not view->spacedInWidth)
        {
            auto viewAnchor = viewEnt->get<UiAnchor>();
            auto currentLeftAnchor = viewAnchor->left;

            for (auto& ent : view->entities)
            {
                if (not ent->has<UiAnchor>())
                {
                    LOG_ERROR(DOM, "Entity " << ent.id << " must have an Anchor component!");
                    continue;
                }

                auto anchor = ent->get<UiAnchor>();

                anchor->setLeftAnchor(currentLeftAnchor);
                anchor->setLeftMargin(view->spacing);

                anchor->setTopAnchor(viewAnchor->top);

                currentLeftAnchor = anchor->right;
            }

            viewAnchor->setRightAnchor(currentLeftAnchor);
            return;
        }

        float currentX = viewUi->x, currentY = viewUi->y, maxHeight = 0.0f;
        size_t nbElementOnCurrentRow = 0;

        for (size_t i = 0; i < view->entities.size(); i++)
        {
            auto ent = view->entities[i];

            if (not ent->has<PositionComponent>())
            {
                LOG_ERROR(DOM, "Entity " << ent.id << " must have a PositionComponent!");
                continue;
            }

            auto pos = ent->get<PositionComponent>();

            if (pos->height > maxHeight)
                maxHeight = pos->height;
        }

        auto totalHeight = 0.0f;

        float currentRowWidth = 0.0f;
        size_t firstRowElementIndex = 0;

        const float maxWidthPos = viewUi->x + viewUi->width;

        for (size_t i = 0; i < view->entities.size(); i++)
        {
            auto ent = view->entities[i];

            if (not ent->has<PositionComponent>())
            {
                LOG_ERROR(DOM, "Entity " << ent.id << " must have a PositionComponent!");
                continue;
            }

            auto pos = ent->get<PositionComponent>();

            if (nbElementOnCurrentRow == 0)
            {
                totalHeight += maxHeight + view->spacing;
            }

            if (nbElementOnCurrentRow != 0 and pos->width >= viewUi->width)
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
            nbElementOnCurrentRow++;
        }

        if (nbElementOnCurrentRow == 0)
        {
            totalHeight += maxHeight + view->spacing;
        }

        viewUi->setHeight(totalHeight);

        if (view->spacedInWidth and (firstRowElementIndex < view->entities.size()))
        {
            // The +1 ensures that spacing is applied to all gaps between elements,
            // including the space before the first and after the last.
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
        auto view = viewEnt->get<HorizontalLayout>();
        view->visible = visible;

        auto& entities = view->entities;

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

    void VerticalLayoutSystem::init()
    {
        auto group = registerGroup<PositionComponent, VerticalLayout>();

        group->addOnGroup([](EntityRef entity) {
            LOG_MILE(DOM, "Add entity " << entity->id << " to ui - vLayout group!");

            auto vLayout = entity->get<VerticalLayout>();
            auto position = entity->get<PositionComponent>();

            vLayout->visible = position->visible;
        });
    }

    void VerticalLayoutSystem::addEntity(EntityRef viewEnt, _unique_id ui)
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

        auto view = viewEnt->get<VerticalLayout>();
        auto uiAnchor = ent->get<UiAnchor>();

        // Z + 1 so the initial z of the list is for the background
        uiAnchor->setZConstrain(PosConstrain{viewEnt.id, AnchorType::Z, PosOpType::Add, 1});

        view->entities.push_back(ent);
    }

    void VerticalLayoutSystem::removeEntity(EntityRef viewEnt, _unique_id index)
    {
        LOG_INFO(DOM, "Removing Entity " << index);
        auto view = viewEnt->get<VerticalLayout>();

        auto it = std::find_if(view->entities.begin(), view->entities.end(), [index](const EntityRef& entity) { return entity.id == index; });

        if (it != view->entities.end())
        {
            view->entities.erase(it);
            ecsRef->removeEntity(index);
        }
    }

    void VerticalLayoutSystem::recalculateChildrenPos(EntityRef viewEnt)
    {
        auto view = viewEnt->get<VerticalLayout>();
        auto viewUi = viewEnt->get<PositionComponent>();

        // This snippet handles the vertical layout of entities within a parent view when neither `fitToHeight` nor `spacedInHeight` is enabled.
        // It aligns entities vertically by setting their top anchor relative to the previous entity's bottom anchor, with a specified spacing.
        // The left anchor of each entity is aligned with the left anchor of the parent view.
        // Finally, the parent view's bottom anchor is updated to encompass all child entities.
        if (not view->fitToHeight and not view->spacedInHeight)
        {
            auto viewAnchor = viewEnt->get<UiAnchor>();
            auto currentBotAnchor = viewAnchor->top;

            for (auto& ent : view->entities)
            {
                if (not ent->has<UiAnchor>())
                {
                    LOG_ERROR(DOM, "Entity " << ent.id << " must have an Anchor component!");
                    continue;
                }

                auto anchor = ent->get<UiAnchor>();

                anchor->setTopAnchor(currentBotAnchor);
                anchor->setTopMargin(view->spacing);

                anchor->setLeftAnchor(viewAnchor->left);

                currentBotAnchor = anchor->bottom;
            }

            viewAnchor->setBottomAnchor(currentBotAnchor);
            return;
        }

        float currentX = viewUi->x, currentY = viewUi->y, maxWidth = 0.0f;
        size_t nbElementOnCurrentColumn = 0;

        for (size_t i = 0; i < view->entities.size(); i++)
        {
            auto ent = view->entities[i];

            if (not ent->has<PositionComponent>())
            {
                LOG_ERROR(DOM, "Entity " << ent.id << " must have a PositionComponent!");
                continue;
            }

            auto pos = ent->get<PositionComponent>();

            if (pos->width > maxWidth)
                maxWidth = pos->width;
        }

        auto totalWidth = 0.0f;

        float currentColumnHeight = 0.0f;
        size_t firstColumnElementIndex = 0;

        const float maxHeightPos = viewUi->y + viewUi->height;

        for (size_t i = 0; i < view->entities.size(); i++)
        {
            auto ent = view->entities[i];

            if (not ent->has<PositionComponent>())
            {
                LOG_ERROR(DOM, "Entity " << ent.id << " must have a PositionComponent!");
                continue;
            }

            auto pos = ent->get<PositionComponent>();

            if (nbElementOnCurrentColumn == 0)
            {
                totalWidth += maxWidth + view->spacing;
            }

            if (nbElementOnCurrentColumn != 0 and pos->height >= viewUi->height)
            {
                pos->setX(currentX);
                pos->setY(currentY);

                currentX += pos->width + view->spacing;
                firstColumnElementIndex = i + 1;
                continue;
            }

            if (view->fitToHeight and (currentY + pos->height > maxHeightPos))
            {
                currentY = viewUi->y;
                currentX += maxWidth + view->spacing;

                if (view->spacedInHeight)
                {
                    auto columnSpacing = nbElementOnCurrentColumn > 1 ? (viewUi->height - currentColumnHeight) / (nbElementOnCurrentColumn - 1) : (viewUi->height - currentColumnHeight);
                    auto currentColumnY = viewUi->y;

                    for (size_t j = 0; j < nbElementOnCurrentColumn; j++)
                    {
                        auto columnEnt = view->entities[firstColumnElementIndex + j];
                        auto columnPos = columnEnt->get<PositionComponent>();

                        columnPos->setY(currentColumnY);
                        currentColumnY += columnPos->height + columnSpacing;
                    }

                    currentColumnHeight = 0;
                    firstColumnElementIndex = i;
                }

                i--;
                nbElementOnCurrentColumn = 0;
                continue;
            }

            if (view->spacedInHeight)
            {
                currentColumnHeight += pos->height;
            }
            else
            {
                pos->setY(currentY);
            }

            pos->setX(currentX);

            currentY += pos->height + view->spacing;
            nbElementOnCurrentColumn++;
        }

        if (nbElementOnCurrentColumn == 0)
        {
            totalWidth += maxWidth + view->spacing;
        }

        viewUi->setHeight(totalWidth);

        if (view->spacedInHeight and (firstColumnElementIndex < view->entities.size()))
        {
            // The +1 ensures that spacing is applied to all gaps between elements,
            // including the space before the first and after the last.
            auto columnSpacing = (viewUi->height - currentColumnHeight) / (nbElementOnCurrentColumn + 1);
            auto currentColumnY = viewUi->y;

            if (currentColumnHeight > viewUi->height)
                columnSpacing = view->spacing;
            else
                currentColumnY += columnSpacing;

            for (size_t j = 0; j < nbElementOnCurrentColumn; j++)
            {
                auto columnEnt = view->entities[firstColumnElementIndex + j];
                auto columnPos = columnEnt->get<PositionComponent>();

                columnPos->setY(currentColumnY);
                currentColumnY += columnPos->height + columnSpacing;
            }

            currentColumnHeight = 0;
        }
    }

    void VerticalLayoutSystem::updateVisibility(EntityRef viewEnt, bool visible)
    {
        auto view = viewEnt->get<VerticalLayout>();
        view->visible = visible;

        auto& entities = view->entities;

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

    void VerticalLayoutSystem::clear(CompRef<VerticalLayout> view)
    {
        for (auto ent : view->entities)
        {
            ecsRef->removeEntity(ent);
        }

        view->entities.clear();
    }
}