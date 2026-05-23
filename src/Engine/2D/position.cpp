#include "stdafx.h"

#include "position.h"

#include "ECS/entitysystem.h"

namespace pg
{
    namespace
    {
        static constexpr const char * const DOM = "Position";

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
                    if (posComp->visible)
                        return posComp->x + posComp->width;
                    else
                        return posComp->x;
                    break;
                case AnchorType::Bottom:
                    if (posComp->visible)
                        return posComp->y + posComp->height;
                    else
                        return posComp->y;
                    break;
                case AnchorType::VerticalCenter:
                    if (posComp->visible)
                        return posComp->y + posComp->height / 2.0f;
                    else
                        return posComp->y;
                    break;
                case AnchorType::HorizontalCenter:
                    if (posComp->visible)
                        return posComp->x + posComp->width / 2.0f;
                    else
                        return posComp->x;
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
                LOG_MILE("PosConstrain", "Entity " << constrain.id << " does not have a PositionComponent!");
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

    // AnchorType to string map
    const std::map<AnchorType, std::string> AnchorTypeToStringMap = {
        {AnchorType::None, "None"},
        {AnchorType::Top, "Top"},
        {AnchorType::Right, "Right"},
        {AnchorType::Bottom, "Bottom"},
        {AnchorType::Left, "Left"},
        {AnchorType::X, "X"},
        {AnchorType::Y, "Y"},
        {AnchorType::Z, "Z"},
        {AnchorType::Width, "Width"},
        {AnchorType::Height, "Height"},
        {AnchorType::TMargin, "TMargin"},
        {AnchorType::RMargin, "RMargin"},
        {AnchorType::BMargin, "BMargin"},
        {AnchorType::LMargin, "LMargin"},
        {AnchorType::VerticalCenter, "VerticalCenter"},
        {AnchorType::HorizontalCenter, "HorizontalCenter"}
    };

    // String to AnchorType map
    const std::map<std::string, AnchorType> StringToAnchorTypeMap = {
        {"None", AnchorType::None},
        {"Top", AnchorType::Top},
        {"Right", AnchorType::Right},
        {"Bottom", AnchorType::Bottom},
        {"Left", AnchorType::Left},
        {"X", AnchorType::X},
        {"Y", AnchorType::Y},
        {"Z", AnchorType::Z},
        {"Width", AnchorType::Width},
        {"Height", AnchorType::Height},
        {"TMargin", AnchorType::TMargin},
        {"RMargin", AnchorType::RMargin},
        {"BMargin", AnchorType::BMargin},
        {"LMargin", AnchorType::LMargin},
        {"VerticalCenter", AnchorType::VerticalCenter},
        {"HorizontalCenter", AnchorType::HorizontalCenter}
    };

    // PosOpType to string map
    const std::map<PosOpType, std::string> PosOpTypeToStringMap = {
        {PosOpType::None, "None"},
        {PosOpType::Add, "Add"},
        {PosOpType::Sub, "Sub"},
        {PosOpType::Mul, "Mul"},
        {PosOpType::Div, "Div"}
    };

    // String to PosOpType map
    const std::map<std::string, PosOpType> StringToPosOpTypeMap = {
        {"None", PosOpType::None},
        {"Add", PosOpType::Add},
        {"Sub", PosOpType::Sub},
        {"Mul", PosOpType::Mul},
        {"Div", PosOpType::Div}
    };

    // Serialize function for UiAnchor
    template <>
    void serialize(Archive& archive, const UiAnchor& value)
    {
        archive.startSerialization("UiAnchor");

        serialize(archive, "topAnchor", value.topAnchor);
        serialize(archive, "leftAnchor", value.leftAnchor);
        serialize(archive, "rightAnchor", value.rightAnchor);
        serialize(archive, "bottomAnchor", value.bottomAnchor);

        serialize(archive, "hasTopAnchor", value.hasTopAnchor);
        serialize(archive, "hasLeftAnchor", value.hasLeftAnchor);
        serialize(archive, "hasRightAnchor", value.hasRightAnchor);
        serialize(archive, "hasBottomAnchor", value.hasBottomAnchor);

        serialize(archive, "verticalCenterAnchor", value.verticalCenterAnchor);
        serialize(archive, "horizontalCenterAnchor", value.horizontalCenterAnchor);

        serialize(archive, "hasVerticalCenter", value.hasVerticalCenter);
        serialize(archive, "hasHorizontalCenter", value.hasHorizontalCenter);

        serialize(archive, "topMargin", value.topMargin);
        serialize(archive, "leftMargin", value.leftMargin);
        serialize(archive, "rightMargin", value.rightMargin);
        serialize(archive, "bottomMargin", value.bottomMargin);

        serialize(archive, "widthConstrain", value.widthConstrain);
        serialize(archive, "heightConstrain", value.heightConstrain);
        serialize(archive, "zConstrain", value.zConstrain);

        serialize(archive, "hasWidthConstrain", value.hasWidthConstrain);
        serialize(archive, "hasHeightConstrain", value.hasHeightConstrain);
        serialize(archive, "hasZConstrain", value.hasZConstrain);

        archive.endSerialization();
    }

    // Deserialize function for UiAnchor
    template <>
    UiAnchor deserialize(const UnserializedObject& serializedString)
    {
        UiAnchor data;

        defaultDeserialize(serializedString, "topAnchor", data.topAnchor);
        defaultDeserialize(serializedString, "leftAnchor", data.leftAnchor);
        defaultDeserialize(serializedString, "rightAnchor", data.rightAnchor);
        defaultDeserialize(serializedString, "bottomAnchor", data.bottomAnchor);

        defaultDeserialize(serializedString, "hasTopAnchor", data.hasTopAnchor);
        defaultDeserialize(serializedString, "hasLeftAnchor", data.hasLeftAnchor);
        defaultDeserialize(serializedString, "hasRightAnchor", data.hasRightAnchor);
        defaultDeserialize(serializedString, "hasBottomAnchor", data.hasBottomAnchor);

        defaultDeserialize(serializedString, "verticalCenterAnchor", data.verticalCenterAnchor);
        defaultDeserialize(serializedString, "horizontalCenterAnchor", data.horizontalCenterAnchor);

        defaultDeserialize(serializedString, "hasVerticalCenter", data.hasVerticalCenter);
        defaultDeserialize(serializedString, "hasHorizontalCenter", data.hasHorizontalCenter);

        defaultDeserialize(serializedString, "topMargin", data.topMargin);
        defaultDeserialize(serializedString, "leftMargin", data.leftMargin);
        defaultDeserialize(serializedString, "rightMargin", data.rightMargin);
        defaultDeserialize(serializedString, "bottomMargin", data.bottomMargin);

        defaultDeserialize(serializedString, "widthConstrain", data.widthConstrain);
        defaultDeserialize(serializedString, "heightConstrain", data.heightConstrain);
        defaultDeserialize(serializedString, "zConstrain", data.zConstrain);

        defaultDeserialize(serializedString, "hasWidthConstrain", data.hasWidthConstrain);
        defaultDeserialize(serializedString, "hasHeightConstrain", data.hasHeightConstrain);
        defaultDeserialize(serializedString, "hasZConstrain", data.hasZConstrain);

        return data;
    }

    // Serialize function for PosAnchor
    template <>
    void serialize(Archive& archive, const PosAnchor& value)
    {
        archive.startSerialization("PosAnchor");

        // Todo
        // serialize(archive, "id", value.id);
        serialize(archive, "type", AnchorTypeToStringMap.at(value.type)); // Use the map for conversion
        serialize(archive, "value", value.value);

        archive.endSerialization();
    }

    // Deserialize function for PosAnchor
    template <>
    PosAnchor deserialize(const UnserializedObject& serializedString)
    {
        PosAnchor data;

        // defaultDeserialize(serializedString, "id", data.id);
        std::string typeStr;
        defaultDeserialize(serializedString, "type", typeStr);
        data.type = StringToAnchorTypeMap.at(typeStr); // Use the map for conversion
        defaultDeserialize(serializedString, "value", data.value);

        return data;
    }

    // Serialize function for PosConstrain
    template <>
    void serialize(Archive& archive, const PosConstrain& value)
    {
        archive.startSerialization("PosConstrain");

        // serialize(archive, "id", value.id);
        serialize(archive, "type", AnchorTypeToStringMap.at(value.type)); // Use the map for conversion
        serialize(archive, "opType", PosOpTypeToStringMap.at(value.opType)); // Use the map for conversion
        serialize(archive, "opValue", value.opValue);

        archive.endSerialization();
    }

    // Deserialize function for PosConstrain
    template <>
    PosConstrain deserialize(const UnserializedObject& serializedString)
    {
        PosConstrain data;

        // defaultDeserialize(serializedString, "id", data.id);
        std::string typeStr;
        defaultDeserialize(serializedString, "type", typeStr);
        data.type = StringToAnchorTypeMap.at(typeStr); // Use the map for conversion

        std::string opTypeStr;
        defaultDeserialize(serializedString, "opType", opTypeStr);
        data.opType = StringToPosOpTypeMap.at(opTypeStr); // Use the map for conversion

        defaultDeserialize(serializedString, "opValue", data.opValue);

        return data;
    }

    void UiAnchor::setTopAnchor(const PosAnchor& anchor)
    {
        if (hasTopAnchor and topAnchor.id == anchor.id)
            return;

        LOG_THIS(DOM);

        if (hasTopAnchor)
            ecsRef->sendEvent(ClearParentingEvent{topAnchor.id, entityId});

        topAnchor = anchor;
        hasTopAnchor = true;
        ecsRef->sendEvent(ParentingEvent{anchor.id, entityId});
        ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
    }

    void UiAnchor::clearTopAnchor()
    {
        if (hasTopAnchor)
        {
            LOG_THIS(DOM);

            ecsRef->sendEvent(ClearParentingEvent{topAnchor.id, entityId});

            hasTopAnchor = false;
            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }

    void UiAnchor::setLeftAnchor(const PosAnchor& anchor)
    {
        if (hasLeftAnchor and leftAnchor.id == anchor.id)
            return;

        LOG_THIS(DOM);

        if (hasLeftAnchor)
            ecsRef->sendEvent(ClearParentingEvent{leftAnchor.id, entityId});

        leftAnchor = anchor;
        hasLeftAnchor = true;
        ecsRef->sendEvent(ParentingEvent{anchor.id, entityId});
        ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
    }

    void UiAnchor::clearLeftAnchor()
    {
        if (hasLeftAnchor)
        {
            LOG_THIS(DOM);

            ecsRef->sendEvent(ClearParentingEvent{leftAnchor.id, entityId});

            hasLeftAnchor = false;
            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }

    void UiAnchor::setRightAnchor(const PosAnchor& anchor)
    {
        if (hasRightAnchor and rightAnchor.id == anchor.id)
            return;

        LOG_THIS(DOM);

        if (hasRightAnchor)
            ecsRef->sendEvent(ClearParentingEvent{rightAnchor.id, entityId});

        rightAnchor = anchor;
        hasRightAnchor = true;
        ecsRef->sendEvent(ParentingEvent{anchor.id, entityId});
        ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
    }

    void UiAnchor::clearRightAnchor()
    {
        if (hasRightAnchor)
        {
            LOG_THIS(DOM);

            ecsRef->sendEvent(ClearParentingEvent{rightAnchor.id, entityId});

            hasRightAnchor = false;
            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }

    void UiAnchor::setBottomAnchor(const PosAnchor& anchor)
    {
        if (hasBottomAnchor and bottomAnchor.id == anchor.id)
            return;

        LOG_THIS(DOM);

        if (hasBottomAnchor)
            ecsRef->sendEvent(ClearParentingEvent{bottomAnchor.id, entityId});

        bottomAnchor = anchor;
        hasBottomAnchor = true;
        ecsRef->sendEvent(ParentingEvent{anchor.id, entityId});
        ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
    }

    void UiAnchor::clearBottomAnchor()
    {
        if (hasBottomAnchor)
        {
            LOG_THIS(DOM);

            ecsRef->sendEvent(ClearParentingEvent{bottomAnchor.id, entityId});

            hasBottomAnchor = false;
            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }

    void UiAnchor::setVerticalCenter(const PosAnchor& anchor)
    {
        if (hasVerticalCenter and verticalCenterAnchor.id == anchor.id)
            return;

        LOG_THIS(DOM);

        verticalCenterAnchor = anchor;
        hasVerticalCenter = true;
        ecsRef->sendEvent(ParentingEvent{anchor.id, entityId});
        ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
    }

    void UiAnchor::clearVerticalCenter()
    {
        if (hasVerticalCenter)
        {
            LOG_THIS(DOM);

            ecsRef->sendEvent(ClearParentingEvent{verticalCenterAnchor.id, entityId});

            hasVerticalCenter = false;
            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }

    void UiAnchor::setHorizontalCenter(const PosAnchor& anchor)
    {
        if (hasHorizontalCenter and horizontalCenterAnchor.id == anchor.id)
            return;

        LOG_THIS(DOM);

        horizontalCenterAnchor = anchor;
        hasHorizontalCenter = true;
        ecsRef->sendEvent(ParentingEvent{anchor.id, entityId});
        ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
    }

    void UiAnchor::clearHorizontalCenter()
    {
        if (hasHorizontalCenter)
        {
            LOG_THIS(DOM);

            ecsRef->sendEvent(ClearParentingEvent{horizontalCenterAnchor.id, entityId});

            hasHorizontalCenter = false;
            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }

    void UiAnchor::fillIn(const UiAnchor& anchor)
    {
        setTopAnchor(PosAnchor{anchor.entityId, AnchorType::Top});
        setLeftAnchor(PosAnchor{anchor.entityId, AnchorType::Left});
        setRightAnchor(PosAnchor{anchor.entityId, AnchorType::Right});
        setBottomAnchor(PosAnchor{anchor.entityId, AnchorType::Bottom});
    }

    void UiAnchor::fillIn(const UiAnchor* anchor)
    {
        setTopAnchor(PosAnchor{anchor->entityId, AnchorType::Top});
        setLeftAnchor(PosAnchor{anchor->entityId, AnchorType::Left});
        setRightAnchor(PosAnchor{anchor->entityId, AnchorType::Right});
        setBottomAnchor(PosAnchor{anchor->entityId, AnchorType::Bottom});
    }

    void UiAnchor::centeredIn(const UiAnchor& anchor)
    {
        setVerticalCenter(PosAnchor{anchor.entityId, AnchorType::VerticalCenter});
        setHorizontalCenter(PosAnchor{anchor.entityId, AnchorType::HorizontalCenter});
    }

    void UiAnchor::centeredIn(const UiAnchor* anchor)
    {
        setVerticalCenter(PosAnchor{anchor->entityId, AnchorType::VerticalCenter});
        setHorizontalCenter(PosAnchor{anchor->entityId, AnchorType::HorizontalCenter});
    }

    void UiAnchor::clearAnchors()
    {
        clearTopAnchor();
        clearBottomAnchor();
        clearLeftAnchor();
        clearRightAnchor();
        clearVerticalCenter();
        clearHorizontalCenter();
    }

    void UiAnchor::setTopMargin(float value)
    {
        if (areNotAlmostEqual(topMargin, value))
        {
            LOG_THIS(DOM);

            topMargin = value;

            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }

    void UiAnchor::setLeftMargin(float value)
    {
        if (areNotAlmostEqual(leftMargin, value))
        {
            LOG_THIS(DOM);

            leftMargin = value;

            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }

    void UiAnchor::setRightMargin(float value)
    {
        if (areNotAlmostEqual(rightMargin, value))
        {
            LOG_THIS(DOM);

            rightMargin = value;

            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }

    void UiAnchor::setBottomMargin(float value)
    {
        if (areNotAlmostEqual(bottomMargin, value))
        {
            LOG_THIS(DOM);

            bottomMargin = value;

            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }

    // Todo need to create a clear constrain method for the constrains
    void UiAnchor::setWidthConstrain(const PosConstrain& constrain)
    {
        if (hasWidthConstrain and widthConstrain.id == constrain.id)
            return;

        LOG_THIS(DOM);

        widthConstrain = constrain;
        hasWidthConstrain = true;
        ecsRef->sendEvent(ParentingEvent{constrain.id, entityId});
        ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
    }

    void UiAnchor::setHeightConstrain(const PosConstrain& constrain)
    {
        if (hasHeightConstrain and heightConstrain.id == constrain.id)
            return;

        LOG_THIS(DOM);

        heightConstrain = constrain;
        hasHeightConstrain = true;
        ecsRef->sendEvent(ParentingEvent{constrain.id, entityId});
        ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
    }

    void UiAnchor::setZConstrain(const PosConstrain& constrain)
    {
        if (hasZConstrain and zConstrain.id == constrain.id)
            return;

        LOG_THIS(DOM);

        zConstrain = constrain;
        hasZConstrain = true;
        ecsRef->sendEvent(ParentingEvent{constrain.id, entityId});
        ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
    }

    void UiAnchor::onCreation(EntityRef entity)
    {
        ecsRef = entity->world();

        entityId = entity->id;

        top = PosAnchor{entityId, AnchorType::Top, 0.0f};
        left = PosAnchor{entityId, AnchorType::Left, 0.0f};
        right = PosAnchor{entityId, AnchorType::Right, 0.0f};
        bottom = PosAnchor{entityId, AnchorType::Bottom, 0.0f};

        verticalCenter = PosAnchor{entityId, AnchorType::VerticalCenter, 0.0f};
        horizontalCenter = PosAnchor{entityId, AnchorType::HorizontalCenter, 0.0f};
    }

    void UiAnchor::onDeletion(EntityRef)
    {
        ecsRef->sendEvent(RemoveParentedChildEvent{entityId});
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
        auto visible = pos->visible;

        bool anchorChanged = areNotAlmostEqual(top.value, pos->y) or
                             areNotAlmostEqual(left.value, pos->x) or
                             areNotAlmostEqual(right.value, (visible ? (pos->x + pos->width) : pos->x)) or
                             areNotAlmostEqual(bottom.value, (visible ? (pos->y + pos->height) : pos->y));

        top.value = pos->y;
        left.value = pos->x;
        right.value = (visible ? (pos->x + pos->width) : pos->x);
        bottom.value = (visible ? (pos->y + pos->height) : pos->y);
        verticalCenter.value = pos->y + pos->height / 2.0f;
        horizontalCenter.value = pos->x + pos->width / 2.0f;

        if (not ecsRef)
            return anchorChanged;

        updateAnchor(hasTopAnchor, topAnchor);
        updateAnchor(hasLeftAnchor, leftAnchor);
        updateAnchor(hasRightAnchor, rightAnchor);
        updateAnchor(hasBottomAnchor, bottomAnchor);
        updateAnchor(hasVerticalCenter, verticalCenterAnchor);
        updateAnchor(hasHorizontalCenter, horizontalCenterAnchor);

        return anchorChanged;
    }

    void ClippedTo::onCreation(EntityRef entity)
    {
        id = entity->id;
        ecsRef = entity->world();

        ecsRef->sendEvent(ParentingEvent{clipperId, id});
    }

    void ClippedTo::setNewClipper(_unique_id clipperId)
    {
        if (this->clipperId != clipperId)
        {
            // Todo add a remove parenting event here (from the last clipper id)

            this->clipperId = clipperId;
            ecsRef->sendEvent(ParentingEvent{clipperId, id});
        }
    }

    bool PositionComponent::updatefromAnchor(const UiAnchor& anchor)
    {
        float oldX = x;
        float oldY = y;
        float oldZ = z;
        float oldWidth = width;
        float oldHeight = height;

        float topMargin = (visible ? anchor.topMargin : 0.0f);
        float leftMargin = (visible ? anchor.leftMargin : 0.0f);
        float rightMargin = (visible ? anchor.rightMargin : 0.0f);
        float bottomMargin = (visible ? anchor.bottomMargin : 0.0f);

        if (anchor.hasTopAnchor and anchor.hasBottomAnchor)
        {
            this->height = (anchor.bottomAnchor.value - bottomMargin) - (anchor.topAnchor.value + topMargin);
            this->y = anchor.topAnchor.value + topMargin;
        }
        else if (anchor.hasTopAnchor and not anchor.hasBottomAnchor)
        {
            this->y = anchor.topAnchor.value + topMargin;
        }
        else if (not anchor.hasTopAnchor and anchor.hasBottomAnchor)
        {
            this->y = (anchor.bottomAnchor.value - bottomMargin) - this->height;
        }

        if (anchor.hasRightAnchor and anchor.hasLeftAnchor)
        {
            this->width = (anchor.rightAnchor.value - rightMargin) - (anchor.leftAnchor.value + leftMargin);
            this->x = anchor.leftAnchor.value + leftMargin;
        }
        else if (anchor.hasRightAnchor and not anchor.hasLeftAnchor)
        {
            this->x = (anchor.rightAnchor.value - rightMargin) - this->width;
        }
        else if (not anchor.hasRightAnchor and anchor.hasLeftAnchor)
        {
            this->x = anchor.leftAnchor.value + leftMargin;
        }

        // Todo we shouldn't be able to center vertically or horizontally if a basic cardinal anchor is set and vice versa
        // When setting a new anchor we should automatically remove all previously set anchors in conflict with the new one !
        if (anchor.hasVerticalCenter)
        {
            this->y = anchor.verticalCenterAnchor.value - this->height / 2.0f;
        }

        if (anchor.hasHorizontalCenter)
        {
            this->x = anchor.horizontalCenterAnchor.value - this->width / 2.0f;
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

        return areNotAlmostEqual(oldX, x) or areNotAlmostEqual(oldY, y) or areNotAlmostEqual(oldZ, z) or areNotAlmostEqual(oldWidth, width) or areNotAlmostEqual(oldHeight, height);
    }

    void PositionComponent::setVisibility(const bool& value)
    {
        if (visible != value)
    {
        visible = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }
    }

    void PositionComponentSystem::pushChildrenInChange(std::vector<_unique_id>& list, std::unordered_set<_unique_id>& set, _unique_id parentId)
    {
        for (const auto& child : parentalMap[parentId])
        {
            auto inserted = set.insert(child);

            if (inserted.second)
            {
                list.push_back(child);
                pushChildrenInChange(list, set, child);
            }
        }
    }

    void PositionComponentSystem::execute()
    {
        if (changedIdsList.empty())
            return;

        LOG_MILE(DOM, "Settling " << changedIdsList.size() << " dirty entities");

        const size_t dirtyCount = changedIdsList.size();

        // Resolve entities once for the whole pass. Safe within a single execute(): no
        // creation/deletion happens between here and the end of the function, so the
        // CompRef/Entity pointers stay valid.
        struct Node
        {
            _unique_id id;
            CompRef<PositionComponent> pos;
            CompRef<UiAnchor> anchor; // empty() when entity has no UiAnchor
            size_t indeg;
            bool processed;
            bool changed;
        };

        std::unordered_map<_unique_id, Node> nodes;
        nodes.reserve(dirtyCount * 2);

        for (const auto& id : changedIdsList)
        {
            auto entity = ecsRef->getEntity(id);
            if (not entity)
                continue;

            if (not entity->has<PositionComponent>())
            {
                LOG_WARNING(DOM, "Entity " << id << " has no PositionComponent");
                continue;
            }

            Node n;
            n.id = id;
            n.pos = entity->get<PositionComponent>();
            if (entity->has<UiAnchor>())
                n.anchor = entity->get<UiAnchor>();
            n.indeg = 0;
            n.processed = false;
            // Entities entered the dirty set either via a direct setter (which only fires when
            // the value actually changed) or via parent cascade (where the anchor recompute
            // below decides). Default to false; flip to true on actual change.
            n.changed = false;
            nodes.emplace(id, n);
        }

        // Compute in-degree: count of each entity's parents that are also in the dirty set.
        // Edges come from reverseParentalMap (child -> parents).
        for (auto& [id, node] : nodes)
        {
            auto it = reverseParentalMap.find(id);
            if (it == reverseParentalMap.end())
                continue;

            for (const auto& parentId : it->second)
            {
                if (nodes.find(parentId) != nodes.end())
                    ++node.indeg;
            }
        }

        // Kahn's algorithm: process roots first (no dirty parent), then their dependents.
        std::vector<_unique_id> ready;
        ready.reserve(dirtyCount);

        for (const auto& [id, node] : nodes)
        {
            if (node.indeg == 0)
                ready.push_back(id);
        }

        auto processNode = [&](Node& node)
        {
            node.processed = true;

            if (not node.anchor.empty())
            {
                // First pass: refresh own anchor values from the entity's CURRENT pos
                // (potentially stale from last frame) and pull in fresh values from anchored
                // parents (which are already settled because they were processed earlier in
                // topological order).
                bool ownMovedPre = node.anchor->update(node.pos);

                // Recompute pos from the now-fresh anchored references.
                bool posMoved = node.pos->updatefromAnchor(*node.anchor);

                // Second pass: if pos moved, refresh own anchor values *again* so they reflect
                // the new pos. This is what makes single-pass settle work — descendants of this
                // node read its own anchor values (top/left/right/...) when they're processed
                // later in this same execute(), and they need to see post-settle state.
                bool ownMovedPost = false;
                if (posMoved)
                    ownMovedPost = node.anchor->update(node.pos);

                node.changed = ownMovedPre or posMoved or ownMovedPost;
            }
            else
            {
                // No UiAnchor: this entity is in the dirty set because something explicitly
                // signalled a change (setter / direct event). The setter only emits when the
                // value really changed, so treat this as a downstream-visible change.
                node.changed = true;
            }
        };

        size_t processedCount = 0;

        while (not ready.empty())
        {
            _unique_id id = ready.back();
            ready.pop_back();

            auto& node = nodes.at(id);
            processNode(node);
            ++processedCount;

            auto childIt = parentalMap.find(id);
            if (childIt == parentalMap.end())
                continue;

            for (const auto& childId : childIt->second)
            {
                auto cIt = nodes.find(childId);
                if (cIt == nodes.end())
                    continue;

                if (--cIt->second.indeg == 0)
                    ready.push_back(childId);
            }
        }

        // Cycle fallback: parental graph has a cycle for these nodes. Note that a parental
        // cycle does NOT necessarily mean a true data-flow cycle: e.g. a Prefab container
        // constrains its size from the leaf (size flows up), while the leaf anchors its
        // position to the container (position flows down). The two depend on each other in
        // the graph, but on disjoint fields, so iteration converges in a handful of passes.
        //
        // Iterate until no node's pos actually moves (quiescence) or a hard cap fires.
        if (processedCount < nodes.size())
        {
            constexpr int CYCLE_PASS_CAP = 8;
            int passes = 0;
            bool anyMoved = true;

            while (anyMoved and passes < CYCLE_PASS_CAP)
            {
                anyMoved = false;
                for (auto& [id, node] : nodes)
                {
                    if (node.processed)
                        continue;

                    if (node.anchor.empty())
                    {
                        node.changed = true;
                        continue;
                    }

                    float oldX = node.pos->x, oldY = node.pos->y;
                    float oldW = node.pos->width, oldH = node.pos->height;
                    float oldZ = node.pos->z;

                    node.anchor->update(node.pos);
                    bool posMoved = node.pos->updatefromAnchor(*node.anchor);
                    if (posMoved)
                        node.anchor->update(node.pos);

                    if (areNotAlmostEqual(oldX, node.pos->x) or areNotAlmostEqual(oldY, node.pos->y)
                        or areNotAlmostEqual(oldW, node.pos->width) or areNotAlmostEqual(oldH, node.pos->height)
                        or areNotAlmostEqual(oldZ, node.pos->z))
                    {
                        anyMoved = true;
                        node.changed = true;
                    }
                }
                ++passes;
            }

            // Mark them processed and warn if we hit the cap (a true unresolvable cycle).
            for (auto& [id, node] : nodes)
                node.processed = true;

            if (passes >= CYCLE_PASS_CAP and anyMoved)
            {
                LOG_WARNING(DOM, "Anchor graph cycle did not converge after "
                            << CYCLE_PASS_CAP << " passes. Likely a real cycle — check anchor setup.");
            }
        }

        // Emit one PositionSettledEvent per entity whose final value actually changed.
        for (const auto& [id, node] : nodes)
        {
            if (node.changed)
                ecsRef->sendEvent(PositionSettledEvent{id});
        }

        changedIdsList.clear();
        changedIdsSet.clear();
    }

    bool inBound(EntityRef entity, float x, float y)
    {
        if (entity.empty() or not entity->has<PositionComponent>())
        {
            LOG_ERROR("Position Component", "Entity[" << entity.id << "] has no Position Component");
            return false;
        }

        auto pos = entity->get<PositionComponent>();

        if (not pos->isRenderable())
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

    // Serialize function for ResizeHandleComponent
    template <>
    void serialize(Archive& archive, const ResizeHandleComponent& value)
    {
        archive.startSerialization("ResizeHandleComponent");

        serialize(archive, "handle", static_cast<int>(value.handle));
        serialize(archive, "handleSize", value.handleSize);
        serialize(archive, "isHovered", value.isHovered);
        serialize(archive, "isDragging", value.isDragging);

        archive.endSerialization();
    }

    // Deserialize function for ResizeHandleComponent
    template <>
    ResizeHandleComponent deserialize(const UnserializedObject& serializedString)
    {
        ResizeHandleComponent data;

        int handleValue = 0;
        defaultDeserialize(serializedString, "handle", handleValue);
        data.handle = static_cast<ResizeHandle>(handleValue);

        defaultDeserialize(serializedString, "handleSize", data.handleSize);
        defaultDeserialize(serializedString, "isHovered", data.isHovered);
        defaultDeserialize(serializedString, "isDragging", data.isDragging);

        return data;
    }

    // Serialize function for RotationHandleComponent
    template <>
    void serialize(Archive& archive, const RotationHandleComponent& value)
    {
        archive.startSerialization("RotationHandleComponent");

        serialize(archive, "handle", static_cast<int>(value.handle));
        serialize(archive, "handleSize", value.handleSize);
        serialize(archive, "distance", value.distance);
        serialize(archive, "isHovered", value.isHovered);
        serialize(archive, "isDragging", value.isDragging);

        archive.endSerialization();
    }

    // Deserialize function for RotationHandleComponent
    template <>
    RotationHandleComponent deserialize(const UnserializedObject& serializedString)
    {
        RotationHandleComponent data;

        int handleValue = 0;
        defaultDeserialize(serializedString, "handle", handleValue);
        data.handle = static_cast<RotationHandle>(handleValue);

        defaultDeserialize(serializedString, "handleSize", data.handleSize);
        defaultDeserialize(serializedString, "distance", data.distance);
        defaultDeserialize(serializedString, "isHovered", data.isHovered);
        defaultDeserialize(serializedString, "isDragging", data.isDragging);

        return data;
    }
}