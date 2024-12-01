#include "listview.h"

#include "scrollable.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "List view";
    }

    void ListView::removeEntity(EntityRef /* entity */)
    {

    }

    void ListView::updateVisibility()
    {
        auto visible = viewUi->isVisible();

        for (auto& ui : entities)
        {
            float childTop    = ui->top;
            float childBottom = ui->bottom;
            float childLeft   = ui->left;
            float childRight  = ui->right;

            ui->visible = false;

            if (visible and (viewUi->inBound(childLeft, childTop) or viewUi->inBound(childLeft, childBottom) or viewUi->inBound(childRight, childTop) or viewUi->inBound(childRight, childBottom)))
            {
                ui->visible = true;
            }

            ui->update();
        }
    }

    void ListView::setVisibility(bool visible)
    {
        viewUi->setVisibility(visible);
        cursorUi->setVisibility(visible);
        sliderUi->setVisibility(visible);

        updateVisibility();
    }

    void ListViewSystem::init()
    {
        
    }

    void ListViewSystem::addEntity(CompRef<ListView> view, CompRef<UiComponent> ui)
    {
        auto& viewUi = view->viewUi;

        if (view->entities.size() > 0)
        {
            ui->setTopAnchor(view->entities.back()->bottom);

            ui->setTopMargin(view->spacing);
        }
        else
        {
            ui->setTopAnchor(viewUi->top);
        }

        ui->setLeftAnchor(viewUi->left);

        // Z + 1 so the initial z of the list is for the background
        ui->setZ(viewUi->pos.z + 1);

        ui->setClipRect({viewUi->top, viewUi->left}, {viewUi->bottom, viewUi->right});

        view->entities.push_back(ui);

        calculateListSize(view);

        if (view->stickToBottom)
        {
            auto cursorUi = view->cursor->get<UiComponent>();

            float maxHeight = viewUi->height - cursorUi->height;

            float currentPos = maxHeight;

            cursorUi->setTopMargin(currentPos);

            if (view->entities.size() > 0)
            {
                view->entities[0]->setTopMargin(-currentPos * static_cast<float>(view->listReelHeight / viewUi->height));
            }
        }

        view->updateVisibility();
    }

    void ListViewSystem::calculateListSize(CompRef<ListView> view)
    {
        view->listReelHeight = 0;

        for (auto ui : view->entities)
        {
            view->listReelHeight += ui->height + view->spacing;
        }

        updateCursorSize(view, view->listReelHeight);
    }

    void ListViewSystem::updateCursorSize(CompRef<ListView> view, const UiSize& maxPos)
    {
        auto height = view->viewUi->height;

        if (maxPos > 0 && height > 0)
        {
            view->cursorHeight = (height / maxPos) * height;
            
            if (view->cursorHeight > height)
                view->cursorHeight = height;
        }
        else
        {
            view->cursorHeight = height;
        }

        view->cursor.get<UiComponent>()->setHeight(view->cursorHeight);
    }

    void ListViewSystem::clear(CompRef<ListView> view)
    {
        for (auto ent : view->entities)
        {
            ecsRef->removeEntity(ent.entityId);
        }

        view->entities.clear();

        view->cursor.get<UiComponent>()->setTopMargin(0);
    }
}