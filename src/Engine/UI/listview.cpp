#include "stdafx.h"

#include "listview.h"

#include "scrollable.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "List view";
    }

    void ListViewSystem::init()
    {
        addListenerToStandardEvent("listviewscroll");

        auto group = registerGroup<PositionComponent, ListView>();

        group->addOnGroup([](EntityRef entity) {
            LOG_MILE(DOM, "Add entity " << entity->id << " to ui - listview group !");

            auto listview = entity->get<ListView>();
            auto position = entity->get<PositionComponent>();

            listview->visible = position->visible;
        });
    }

    void ListViewSystem::onEvent(const StandardEvent& event)
    {
        auto id = event.values.at("id").get<size_t>();

        auto ent = ecsRef->getEntity(id);

        if (not ent or (not ent->has<ListView>()))
        {
            LOG_ERROR(DOM, "Error while looking for entity: " << id);
            return;
        }

        auto viewComp = ent->get<ListView>();
        auto cursorAnchor = viewComp->cursor->get<UiAnchor>();

        auto viewUi = ent->get<PositionComponent>();
        auto cursorUi = viewComp->cursor->get<PositionComponent>();

        auto currentPos = cursorAnchor->topMargin - event.values.at("y").get<int>() * viewComp->scrollSpeed;

        if (currentPos < 0)
            currentPos = 0;

        float maxHeight = viewUi->height - cursorUi->height;

        if (currentPos > maxHeight)
            currentPos = maxHeight;

        cursorAnchor->setTopMargin(currentPos);

        if (viewComp->entities.size() > 0)
        {
            if (viewComp->entities[0]->has<UiAnchor>())
            {
                viewComp->entities[0]->get<UiAnchor>()->setTopMargin(-currentPos * viewComp->listReelHeight / viewUi->height);
            }
            else
            {
                LOG_ERROR("ListViewSystem", "First entity in the list [" << id << "] must have a UiAnchor!");
            }
        }

    };

    void ListView::removeEntity(EntityRef /* entity */)
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

        auto uiAnchor = ent->get<UiAnchor>();

        auto bodySizerAnchor = view->bodySizer->get<UiAnchor>();

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

            bodySizerAnchor->setBottomAnchor(uiAnchor->bottom);
        }
        else
        {
            uiAnchor->setTopAnchor(viewAnchor->top);

            bodySizerAnchor->setTopAnchor(uiAnchor->top);
            bodySizerAnchor->setBottomAnchor(uiAnchor->bottom);
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

        updateVisibility(viewEnt, viewUi->visible);
    }

    void ListViewSystem::calculateListSize(CompRef<ListView> view)
    {
        view->listReelHeight = view->bodySizer->get<PositionComponent>()->height;

        updateCursorSize(view, view->listReelHeight);

        auto ent = view.getEntity();

        updateVisibility(ent, ent->get<PositionComponent>()->visible);
    }

    void ListViewSystem::updateCursorSize(CompRef<ListView> view, float maxPos)
    {
        auto viewEnt = view.getEntity();

        if (not viewEnt or not viewEnt->has<PositionComponent>())
        {
            LOG_ERROR("ListViewSystem", "Entity " << view.entityId << " must have a PositionComponent!");
            return;
        }

        auto height = viewEnt->get<PositionComponent>()->height;

        if (maxPos > 0 and height > 0)
        {
            view->cursorHeight = (height / maxPos) * height;

            if (view->cursorHeight > height)
            view->cursorHeight = height;
        }
        else
        {
            view->cursorHeight = height;
        }

        LOG_MILE("ListViewSystem", "Height: " << height << ", MaxPos: " << maxPos << " Cursor height: " << view->cursorHeight);

        view->cursor.get<PositionComponent>()->setHeight(view->cursorHeight);
    }

    void ListViewSystem::updateVisibility(EntityRef viewEnt, bool visible)
    {
        auto& entities = viewEnt->get<ListView>()->entities;

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