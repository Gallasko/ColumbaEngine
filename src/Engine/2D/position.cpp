#include "position.h"

#include "ECS/entitysystem.h"

namespace pg
{
    namespace
    {
        float getValueFromType(CompRef<PositionComponent> posComp, const AnchorType& dir)
        {
            switch (dir)
            {
                case AnchorType::Top:
                    return posComp->y;
                    break;
                case AnchorType::Left:
                    return posComp->x;
                    break;
                case AnchorType::Right:
                    return posComp->x + posComp->width;
                    break;
                case AnchorType::Bottom:
                    return posComp->y + posComp->height;
                    break;
                default:
                    // Todo add support for width, height, center alignment ... to this getter
                    LOG_ERROR("UiAnchor", "Invalid anchor type, type is not yet managed");
                    return 0.0f;
                    break;
            }
        }
    }

    void UiAnchor::setTopAnchor(const PosAnchor& anchor)
    {
        topAnchor = anchor;
        hasTopAnchor = true;
        ecsRef->sendEvent(ParentingEvent{anchor.id, id});
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::clearTopAnchor()
    {
        hasTopAnchor = false;
        // Todo send remove parenting event
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::setLeftAnchor(const PosAnchor& anchor)
    {
        leftAnchor = anchor;
        hasLeftAnchor = true;
        ecsRef->sendEvent(ParentingEvent{anchor.id, id});
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::clearLeftAnchor()
    {
        hasLeftAnchor = false;
        // Todo send remove parenting event
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::setRightAnchor(const PosAnchor& anchor)
    {
        rightAnchor = anchor;
        hasRightAnchor = true;
        ecsRef->sendEvent(ParentingEvent{anchor.id, id});
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::clearRightAnchor()
    {
        hasRightAnchor = false;
        // Todo send remove parenting event
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::setBottomAnchor(const PosAnchor& anchor)
    {
        bottomAnchor = anchor;
        hasBottomAnchor = true;
        ecsRef->sendEvent(ParentingEvent{anchor.id, id});
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::clearBottomAnchor()
    {
        hasBottomAnchor = false;
        // Todo send remove parenting event
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::setTopMargin(float value)
    {
        topMargin = value;
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::setLeftMargin(float value)
    {
        leftMargin = value;
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::setRightMargin(float value)
    {
        rightMargin = value;
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::setBottomMargin(float value)
    {
        bottomMargin = value;
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::setWidthConstrain(const PosConstrain& constrain)
    {
        widthConstrain = constrain;
        ecsRef->sendEvent(ParentingEvent{constrain.id, id});
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::setHeightConstrain(const PosConstrain& constrain)
    {
        heightConstrain = constrain;
        ecsRef->sendEvent(ParentingEvent{constrain.id, id});
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::onCreation(EntityRef entity)
    {
        ecsRef = entity->world();

        id = entity->id;

        top = PosAnchor{id, AnchorType::Top, 0.0f};
        left = PosAnchor{id, AnchorType::Left, 0.0f};
        right = PosAnchor{id, AnchorType::Right, 0.0f};
        bottom = PosAnchor{id, AnchorType::Bottom, 0.0f};
    }

    void UiAnchor::updateAnchor(bool hasAnchor, PosAnchor& anchor)
    {
        if (hasAnchor)
        {
            auto ent = ecsRef->getEntity(anchor.id);

            if (ent and ent->has<PositionComponent>())
            {
                anchor.value = getValueFromType(ent->get<PositionComponent>(), anchor.type);
            }
        }
    }

    void UiAnchor::update()
    {
        auto entity = ecsRef->getEntity(id);

        if (entity and entity->has<PositionComponent>())
        {
            auto pos = entity->get<PositionComponent>();

            top.value = pos->y;
            left.value = pos->x;
            right.value = pos->x + pos->width;
            bottom.value = pos->y + pos->height;
        }

        updateAnchor(hasTopAnchor, topAnchor);
        updateAnchor(hasLeftAnchor, leftAnchor);
        updateAnchor(hasRightAnchor, rightAnchor);
        updateAnchor(hasBottomAnchor, bottomAnchor);
    }

    void ClippedTo::onCreation(EntityRef entity)
    {
        id = entity->id;
        ecsRef = entity->world();

        ecsRef->sendEvent(ParentingEvent{clipperId, id});
        ecsRef->sendEvent(EntityChangedEvent{id});
    }

    void ClippedTo::setNewClipper(_unique_id clipperId)
    {
        if (this->clipperId != clipperId)
        {
            // Todo add a remove parenting event here (from the last clipper id)

            this->clipperId = clipperId;
            ecsRef->sendEvent(ParentingEvent{clipperId, id});
            ecsRef->sendEvent(EntityChangedEvent{id});
        }
    }

    void PositionComponent::onCreation(EntityRef entity)
    {
        ecsRef = entity->world();
        id = entity->id;
    }

    void PositionComponent::setX(float x)
    {
        if (this->x != x)
        {
            this->x = x;
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setY(float y)
    {
        if (this->y != y)
        {
            this->y = y;
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setZ(float z)
    {
        if (this->z != z)
        {
            this->z = z;
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setWidth(float width)
    {
        if (this->width != width)
        {
            this->width = width;
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setHeight(float height)
    {
        if (this->height != height)
        {
            this->height = height;
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setRotation(float rotation)
    {
        if (this->rotation != rotation)
        {
            this->rotation = rotation;
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setVisibility(bool visible)
    {
        if (this->visible != visible)
        {
            this->visible = visible;
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    bool PositionComponent::updatefromAnchor(const UiAnchor& anchor)
    {
        float oldX = x;
        float oldY = y;

        if (anchor.hasTopAnchor and anchor.hasBottomAnchor)
        {
            this->height = (anchor.bottomAnchor.value - anchor.bottomMargin) - (anchor.topAnchor.value - anchor.topMargin);
            this->y = anchor.topAnchor.value + anchor.topMargin;
        }
        else if (anchor.hasTopAnchor and not anchor.hasBottomAnchor)
        {
            this->y = anchor.topAnchor.value + anchor.topMargin;
        }
        else if (not anchor.hasTopAnchor and anchor.hasBottomAnchor)
        {
            this->y = (anchor.bottomAnchor.value - anchor.bottomMargin) - this->height;
        }

        if (anchor.hasRightAnchor and anchor.hasLeftAnchor)
        {
            this->width = (anchor.rightAnchor.value - anchor.rightMargin) - (anchor.leftAnchor.value - anchor.leftMargin);
            this->x = anchor.leftAnchor.value + anchor.leftMargin;
        }
        else if (anchor.hasRightAnchor and not anchor.hasLeftAnchor)
        {
            this->x = (anchor.rightAnchor.value - anchor.rightMargin) - this->width;
        }
        else if (not anchor.hasRightAnchor and anchor.hasLeftAnchor)
        {
            this->x = anchor.leftAnchor.value + anchor.leftMargin;
        }

        return oldX != x or oldY != y;
    }

    void PositionComponentSystem::pushChildrenInChange(_unique_id parentId, Entity *entity)
    {
        if (entity->has<UiAnchor>())
        {
            entity->get<UiAnchor>()->update();

            // Todo check
            // If the position component get changed by the anchor moving then we push its children to the queue for check
            entity->get<PositionComponent>()->updatefromAnchor(*entity->get<UiAnchor>());
        }
        // Todo check else case should never happen, as if the entity id is in the parentalMap, then the entity should have UiAnchor component.

        for (const auto& child : parentalMap[parentId])
        {
            auto childEntity = ecsRef->getEntity(child);

            // Todo, should be impossible to have a child without Position component or non existant.
            if (childEntity and childEntity->has<PositionComponent>())
            {
                changedIds.insert(child);

                pushChildrenInChange(child, childEntity);
            }
        }
    }
    
    void PositionComponentSystem::execute()
    {
        while (not eventQueue.empty())
        {
            const auto& event = eventQueue.front();
        
            auto entity = ecsRef->getEntity(event.id);

            if (not entity or not entity->has<PositionComponent>())
            {
                eventQueue.pop();
                continue;
            }

            if (not changedIds.count(event.id))
            {
                changedIds.insert(event.id);
                pushChildrenInChange(event.id, entity);
            }
        
            eventQueue.pop();
        }

        if (changedIds.size() > 0)
        {
            for (const auto& id : changedIds)
            {
                ecsRef->sendEvent(EntityChangedEvent{id});
            }

            changedIds.clear();
        }
    }
}