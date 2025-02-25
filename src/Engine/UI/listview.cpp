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
        // auto visible = viewUi->isVisible();

        // for (auto& ui : entities)
        // {
        //     ui->visible = false;

        //     if (visible)
        //     {
        //         float childTop    = ui->top;
        //         float childBottom = ui->bottom;
        //         float childLeft   = ui->left;
        //         float childRight  = ui->right;

        //         if (viewUi->inBound(childLeft, childTop) or viewUi->inBound(childLeft, childBottom) or viewUi->inBound(childRight, childTop) or viewUi->inBound(childRight, childBottom))
        //         {
        //             ui->visible = true;
        //         }
        //     }

        //     ui->update();
        // }
    }

    void ListView::setVisibility(bool visible)
    {
        // viewUi->setVisibility(visible);
        // cursorUi->setVisibility(visible);
        // sliderUi->setVisibility(visible);

        // updateVisibility();
    }

    void ListViewSystem::init()
    {
        
    }

    void ListViewSystem::addEntity(EntityRef viewEnt, _unique_id ui)
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

        auto view = viewEnt->get<ListView>();
        auto viewUi = viewEnt->get<PositionComponent>();
        auto viewAnchor = viewEnt->get<UiAnchor>();

        auto uiPos = ent->get<PositionComponent>();
        auto uiAnchor = ent->get<UiAnchor>();

        if (view->entities.size() > 0)
        {
            auto lastUi = view->entities.back();

            if (lastUi->has<UiAnchor>())
            {
                uiAnchor->setTopAnchor(lastUi->get<UiAnchor>()->bottom);
            }
            else
            {
                LOG_ERROR("ListViewSystem", "Last entity in the list [" << viewEnt.id << "] must have a UiAnchor!");
                uiAnchor->setTopAnchor(viewAnchor->top);
            }

            uiAnchor->setTopMargin(view->spacing);
        }
        else
        {
            uiAnchor->setTopAnchor(viewAnchor->top);
        }

        uiAnchor->setLeftAnchor(viewAnchor->left);

        // Z + 1 so the initial z of the list is for the background
        uiAnchor->setZConstrain(PosConstrain{viewEnt.id, AnchorType::Z, PosOpType::Add, 1});

        ecsRef->attach<ClippedTo>(ent, viewUi->id);

        view->entities.push_back(ent);

        calculateListSize(view);

        if (view->stickToBottom)
        {
            auto cursorUi = view->cursor->get<PositionComponent>();

            float maxHeight = viewUi->height - cursorUi->height;

            float currentPos = maxHeight;

            view->cursor->get<UiAnchor>()->setTopMargin(currentPos);

            if (view->entities.size() > 0)
            {
                if (view->entities[0]->has<UiAnchor>())
                {
                    view->entities[0]->get<UiAnchor>()->setTopMargin(-currentPos * view->listReelHeight / viewUi->height);
                }
                else
                {
                    LOG_ERROR("ListViewSystem", "First entity in the list [" << viewEnt.id << "] must have a UiAnchor!");
                }                    
            }
        }

        view->updateVisibility();
    }

    void ListViewSystem::calculateListSize(CompRef<ListView> view)
    {
        view->listReelHeight = 0;

        for (auto ui : view->entities)
        {
            if (ui->has<PositionComponent>())
                view->listReelHeight += ui->get<PositionComponent>()->height + view->spacing;
        }

        updateCursorSize(view, view->listReelHeight);
    }

    void ListViewSystem::updateCursorSize(CompRef<ListView> view, const UiSize& maxPos)
    {
        auto viewEnt = view.getEntity();

        if (not viewEnt or not viewEnt->has<PositionComponent>())
        {
            LOG_ERROR("ListViewSystem", "Entity " << view.entityId << " must have a PositionComponent!");
            return;
        }

        auto height = viewEnt->get<PositionComponent>()->height;

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

        view->cursor.get<PositionComponent>()->setHeight(view->cursorHeight);
    }

    void ListViewSystem::clear(CompRef<ListView> view)
    {
        for (auto ent : view->entities)
        {
            ecsRef->removeEntity(ent);
        }

        view->entities.clear();

        view->cursor.get<UiAnchor>()->setTopMargin(0);
    }
}