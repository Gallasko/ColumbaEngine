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

        float constrainCalculation(EntitySystem* ecsRef, const PosConstrain& constrain)
        {
            auto entity = ecsRef->getEntity(constrain.id);

            if (not entity or not entity->has<PositionComponent>())
            {
                LOG_ERROR("PosConstrain", "Entity " << constrain.id << " does not have a PositionComponent!");
                return 0.0f;
            }

            auto pos = entity->get<PositionComponent>();

            float value = 0.0f;

            switch (constrain.type)
            {
            case AnchorType::Width:
                value = pos->width;
                break;

            case AnchorType::Height:
                value = pos->height;
                break;
            
            case AnchorType::X:
                value = pos->x;
                break;

            case AnchorType::Y:
                value = pos->y;
                break;

            case AnchorType::Z:
                value = pos->z;
                break;

            default:
                LOG_ERROR("UiAnchor", "Invalid anchor type, type is not yet managed");
                break;
            }

            switch (constrain.opType)
            {
            case PosOpType::Add:
                value += constrain.opValue;
                break;

            case PosOpType::Sub:
                value -= constrain.opValue;
                break;

            case PosOpType::Mul:
                value *= constrain.opValue;
                break;

            case PosOpType::Div:
                if (constrain.opValue != 0.0f)
                    value /= constrain.opValue;
                else
                {
                    LOG_ERROR("UiAnchor", "Division by zero"); 
                }
                break;

            case PosOpType::None:
            default:
                // We do nothing
                break;
            }

            return value;
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
        hasWidthConstrain = true;
        ecsRef->sendEvent(ParentingEvent{constrain.id, id});
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::setHeightConstrain(const PosConstrain& constrain)
    {
        heightConstrain = constrain;
        hasHeightConstrain = true;
        ecsRef->sendEvent(ParentingEvent{constrain.id, id});
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::setZConstrain(const PosConstrain& constrain)
    {
        zConstrain = constrain;
        hasZConstrain = true;
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

    bool UiAnchor::update(CompRef<PositionComponent> pos)
    {
        bool anchorChanged = top.value != pos->y or 
                             left.value != pos->x or
                             right.value != pos->x + pos->width or
                             bottom.value != pos->y + pos->height;

        top.value = pos->y;
        left.value = pos->x;
        right.value = pos->x + pos->width;
        bottom.value = pos->y + pos->height;

        if (not ecsRef)
            return anchorChanged;

        updateAnchor(hasTopAnchor, topAnchor);
        updateAnchor(hasLeftAnchor, leftAnchor);
        updateAnchor(hasRightAnchor, rightAnchor);
        updateAnchor(hasBottomAnchor, bottomAnchor);

        return anchorChanged;
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

            if (ecsRef)
                ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setY(float y)
    {
        if (this->y != y)
        {
            this->y = y;

            if (ecsRef)
                ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setZ(float z)
    {
        if (this->z != z)
        {
            this->z = z;

            if (ecsRef)
                ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setWidth(float width)
    {
        if (this->width != width)
        {
            this->width = width;

            if (ecsRef)
                ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setHeight(float height)
    {
        if (this->height != height)
        {
            this->height = height;

            if (ecsRef)
                ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setRotation(float rotation)
    {
        if (this->rotation != rotation)
        {
            this->rotation = rotation;
            
            if (ecsRef)
                ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setVisibility(bool visible)
    {
        if (this->visible != visible)
        {
            this->visible = visible;

            if (ecsRef)
                ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    bool PositionComponent::updatefromAnchor(const UiAnchor& anchor)
    {
        float oldX = x;
        float oldY = y;
        float oldZ = z;
        float oldWidth = width;
        float oldHeight = height;

        if (anchor.hasTopAnchor and anchor.hasBottomAnchor)
        {
            this->height = (anchor.bottomAnchor.value - anchor.bottomMargin) - (anchor.topAnchor.value + anchor.topMargin);
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
            this->width = (anchor.rightAnchor.value - anchor.rightMargin) - (anchor.leftAnchor.value + anchor.leftMargin);
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

        // Cannot do constrain calculation if we don't have access to ecsRef
        if (ecsRef)
        {
            if (anchor.hasZConstrain)
                z = constrainCalculation(ecsRef, anchor.zConstrain);

            if (anchor.hasWidthConstrain)
                width = constrainCalculation(ecsRef, anchor.widthConstrain);

            if (anchor.hasHeightConstrain)
                height = constrainCalculation(ecsRef, anchor.heightConstrain);
        }

        return oldX != x or oldY != y or oldZ != z or oldWidth != width or oldHeight != height;
    }

    void PositionComponentSystem::pushChildrenInChange(_unique_id parentId)
    {
        for (const auto& child : parentalMap[parentId])
        {
            auto inserted = changedIds.insert(child);

            if (inserted.second)
                pushChildrenInChange(child);
        }
    }
    
    void PositionComponentSystem::execute()
    {
        while (not eventQueue.empty())
        {
            const auto& event = eventQueue.front();
        
            if (not changedIds.count(event.id))
            {
                changedIds.insert(event.id);
                pushChildrenInChange(event.id);
            }
        
            eventQueue.pop();
        }

        if (changedIds.size() > 0)
        {
            for (const auto& id : changedIds)
            {
                auto anchorChanged = false;
                auto entity = ecsRef->getEntity(id);

                if (not entity or not entity->has<PositionComponent>())
                    continue;

                if (entity->has<UiAnchor>())
                {
                    auto anchor = entity->get<UiAnchor>();

                    auto pos = entity->get<PositionComponent>();
                    
                    anchorChanged = anchor->update(pos);

                    // Todo check
                    // If the position component get changed by the anchor moving then we push its children to the queue for check
                    pos->updatefromAnchor(*anchor);
                }

                ecsRef->sendEvent(EntityChangedEvent{id});

                if (anchorChanged)
                    ecsRef->sendEvent(PositionComponentChangedEvent{id});
            }

            changedIds.clear();
        }
    }

    bool inBound(EntityRef entity, float x, float y)
    {
        if (entity.empty() or not entity->has<PositionComponent>())
        {
            LOG_ERROR("Position Component", "Entity[" << entity.id << "] has no Position Component");
            return false;
        }

        auto pos = entity->get<PositionComponent>();

        if (not pos->visible)
        {
            return false;
        }

        return x >= pos->x and x <= pos->x + pos->width and y >= pos->y and y <= pos->y + pos->height;
    }

    bool inClipBound(EntityRef entity, float x, float y)
    {
        // We first check if the pos is in the entity 
        auto inEntityBound = inBound(entity, x, y);

        // Early exit if the pos is not in the entity
        if (not inEntityBound)
            return false;

        // If the entity is not clipped to anything we just devolve to standard bound test
        if (not entity->has<ClippedTo>())
        {
            return inEntityBound;
        }
        
        // If the entity is clipped to something, then we just check if the pos is also in the clipper's bound
        auto clipper = entity->world()->getEntity(entity->get<ClippedTo>()->clipperId);

        return inBound(clipper, x, y);
    }
}