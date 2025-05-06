#include "position.h"

#include "ECS/entitysystem.h"

namespace pg
{
    namespace
    {
        static constexpr const char * const DOM = "Position";

        constexpr float EPSILON = 1e-5f;

        bool areAlmostEqual(float a, float b, float epsilon = EPSILON)
        {
            return std::fabs(a - b) <= epsilon * std::max({1.0f, std::fabs(a), std::fabs(b)});
        }

        bool areNotAlmostEqual(float a, float b, float epsilon = EPSILON)
        {
            return not areAlmostEqual(a, b, epsilon);
        }

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

    // Serialize function for PositionComponent
    template <>
    void serialize(Archive& archive, const PositionComponent& value)
    {
        archive.startSerialization("PositionComponent");

        serialize(archive, "x", value.x);
        serialize(archive, "y", value.y);
        serialize(archive, "z", value.z);

        serialize(archive, "width", value.width);
        serialize(archive, "height", value.height);

        serialize(archive, "rotation", value.rotation);
        serialize(archive, "visible", value.visible);

        archive.endSerialization();
    }

    // Deserialize function for PositionComponent
    template <>
    PositionComponent deserialize(const UnserializedObject& serializedString)
    {
        PositionComponent data;

        defaultDeserialize(serializedString, "x", data.x);
        defaultDeserialize(serializedString, "y", data.y);
        defaultDeserialize(serializedString, "z", data.z);

        defaultDeserialize(serializedString, "width", data.width);
        defaultDeserialize(serializedString, "height", data.height);

        defaultDeserialize(serializedString, "rotation", data.rotation);
        defaultDeserialize(serializedString, "visible", data.visible);

        return data;
    }

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

        serialize(archive, "id", value.id);
        serialize(archive, "type", AnchorTypeToStringMap.at(value.type)); // Use the map for conversion
        serialize(archive, "value", value.value);

        archive.endSerialization();
    }

    // Deserialize function for PosAnchor
    template <>
    PosAnchor deserialize(const UnserializedObject& serializedString)
    {
        PosAnchor data;

        defaultDeserialize(serializedString, "id", data.id);
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

        serialize(archive, "id", value.id);
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

        defaultDeserialize(serializedString, "id", data.id);
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
            ecsRef->sendEvent(ClearParentingEvent{topAnchor.id, id});

        topAnchor = anchor;
        hasTopAnchor = true;
        ecsRef->sendEvent(ParentingEvent{anchor.id, id});
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::clearTopAnchor()
    {
        if (hasTopAnchor)
        {
            LOG_THIS(DOM);

            ecsRef->sendEvent(ClearParentingEvent{topAnchor.id, id});

            hasTopAnchor = false;
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void UiAnchor::setLeftAnchor(const PosAnchor& anchor)
    {
        if (hasLeftAnchor and leftAnchor.id == anchor.id)
            return;

        LOG_THIS(DOM);

        if (hasLeftAnchor)
            ecsRef->sendEvent(ClearParentingEvent{leftAnchor.id, id});

        leftAnchor = anchor;
        hasLeftAnchor = true;
        ecsRef->sendEvent(ParentingEvent{anchor.id, id});
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::clearLeftAnchor()
    {
        if (hasLeftAnchor)
        {
            LOG_THIS(DOM);

            ecsRef->sendEvent(ClearParentingEvent{leftAnchor.id, id});

            hasLeftAnchor = false;
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void UiAnchor::setRightAnchor(const PosAnchor& anchor)
    {
        if (hasRightAnchor and rightAnchor.id == anchor.id)
            return;

        LOG_THIS(DOM);

        if (hasRightAnchor)
            ecsRef->sendEvent(ClearParentingEvent{rightAnchor.id, id});

        rightAnchor = anchor;
        hasRightAnchor = true;
        ecsRef->sendEvent(ParentingEvent{anchor.id, id});
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::clearRightAnchor()
    {
        if (hasRightAnchor)
        {
            LOG_THIS(DOM);

            ecsRef->sendEvent(ClearParentingEvent{rightAnchor.id, id});

            hasRightAnchor = false;
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void UiAnchor::setBottomAnchor(const PosAnchor& anchor)
    {
        if (hasBottomAnchor and bottomAnchor.id == anchor.id)
            return;

        LOG_THIS(DOM);

        if (hasBottomAnchor)
            ecsRef->sendEvent(ClearParentingEvent{bottomAnchor.id, id});

        bottomAnchor = anchor;
        hasBottomAnchor = true;
        ecsRef->sendEvent(ParentingEvent{anchor.id, id});
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::clearBottomAnchor()
    {
        if (hasBottomAnchor)
        {
            LOG_THIS(DOM);

            ecsRef->sendEvent(ClearParentingEvent{bottomAnchor.id, id});

            hasBottomAnchor = false;
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void UiAnchor::setVerticalCenter(const PosAnchor& anchor)
    {
        if (hasVerticalCenter and verticalCenterAnchor.id == anchor.id)
            return;

        LOG_THIS(DOM);

        verticalCenterAnchor = anchor;
        hasVerticalCenter = true;
        ecsRef->sendEvent(ParentingEvent{anchor.id, id});
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::clearVerticalCenter()
    {
        if (hasVerticalCenter)
        {
            LOG_THIS(DOM);

            ecsRef->sendEvent(ClearParentingEvent{verticalCenterAnchor.id, id});

            hasVerticalCenter = false;
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void UiAnchor::setHorizontalCenter(const PosAnchor& anchor)
    {
        if (hasHorizontalCenter and horizontalCenterAnchor.id == anchor.id)
            return;

        LOG_THIS(DOM);

        horizontalCenterAnchor = anchor;
        hasHorizontalCenter = true;
        ecsRef->sendEvent(ParentingEvent{anchor.id, id});
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::clearHorizontalCenter()
    {
        if (hasHorizontalCenter)
        {
            LOG_THIS(DOM);

            ecsRef->sendEvent(ClearParentingEvent{horizontalCenterAnchor.id, id});

            hasHorizontalCenter = false;
            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void UiAnchor::fillIn(const UiAnchor& anchor)
    {
        setTopAnchor(PosAnchor{anchor.id, AnchorType::Top});
        setLeftAnchor(PosAnchor{anchor.id, AnchorType::Left});
        setRightAnchor(PosAnchor{anchor.id, AnchorType::Right});
        setBottomAnchor(PosAnchor{anchor.id, AnchorType::Bottom});
    }

    void UiAnchor::fillIn(const UiAnchor* anchor)
    {
        setTopAnchor(PosAnchor{anchor->id, AnchorType::Top});
        setLeftAnchor(PosAnchor{anchor->id, AnchorType::Left});
        setRightAnchor(PosAnchor{anchor->id, AnchorType::Right});
        setBottomAnchor(PosAnchor{anchor->id, AnchorType::Bottom});
    }

    void UiAnchor::centeredIn(const UiAnchor& anchor)
    {
        setVerticalCenter(PosAnchor{anchor.id, AnchorType::VerticalCenter});
        setHorizontalCenter(PosAnchor{anchor.id, AnchorType::HorizontalCenter});
    }

    void UiAnchor::centeredIn(const UiAnchor* anchor)
    {
        setVerticalCenter(PosAnchor{anchor->id, AnchorType::VerticalCenter});
        setHorizontalCenter(PosAnchor{anchor->id, AnchorType::HorizontalCenter});
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

            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void UiAnchor::setLeftMargin(float value)
    {
        if (areNotAlmostEqual(leftMargin, value))
        {
            LOG_THIS(DOM);

            leftMargin = value;

            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void UiAnchor::setRightMargin(float value)
    {
        if (areNotAlmostEqual(rightMargin, value))
        {
            LOG_THIS(DOM);

            rightMargin = value;

            ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void UiAnchor::setBottomMargin(float value)
    {
        if (areNotAlmostEqual(bottomMargin, value))
        {
            LOG_THIS(DOM);

            bottomMargin = value;

            ecsRef->sendEvent(PositionComponentChangedEvent{id});
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
        ecsRef->sendEvent(ParentingEvent{constrain.id, id});
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::setHeightConstrain(const PosConstrain& constrain)
    {
        if (hasHeightConstrain and heightConstrain.id == constrain.id)
            return;

        LOG_THIS(DOM);

        heightConstrain = constrain;
        hasHeightConstrain = true;
        ecsRef->sendEvent(ParentingEvent{constrain.id, id});
        ecsRef->sendEvent(PositionComponentChangedEvent{id});
    }

    void UiAnchor::setZConstrain(const PosConstrain& constrain)
    {
        if (hasZConstrain and zConstrain.id == constrain.id)
            return;

        LOG_THIS(DOM);

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

        verticalCenter = PosAnchor{id, AnchorType::VerticalCenter, 0.0f};
        horizontalCenter = PosAnchor{id, AnchorType::HorizontalCenter, 0.0f};
    }

    void UiAnchor::onDeletion(EntityRef)
    {
        clearAnchors();

        // Todo clear constrains
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
        if (areNotAlmostEqual(this->x, x))
        {
            LOG_THIS(DOM);

            this->x = x;

            if (ecsRef)
                ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setY(float y)
    {
        if (areNotAlmostEqual(this->y, y))
        {
            LOG_THIS(DOM);

            this->y = y;

            if (ecsRef)
                ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setZ(float z)
    {
        if (areNotAlmostEqual(this->z, z))
        {
            LOG_THIS(DOM);

            this->z = z;

            if (ecsRef)
                ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setWidth(float width)
    {
        if (areNotAlmostEqual(this->width, width))
        {
            LOG_THIS(DOM);

            this->width = width;

            if (ecsRef)
                ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setHeight(float height)
    {
        if (areNotAlmostEqual(this->height, height))
        {
            LOG_THIS(DOM);

            this->height = height;

            if (ecsRef)
                ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setRotation(float rotation)
    {
        if (areNotAlmostEqual(this->rotation, rotation))
        {
            LOG_THIS(DOM);

            this->rotation = rotation;

            if (ecsRef)
                ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setVisibility(bool visible)
    {
        if (this->visible != visible)
        {
            LOG_THIS(DOM);

            this->visible = visible;

            if (ecsRef)
                ecsRef->sendEvent(PositionComponentChangedEvent{id});
        }
    }

    void PositionComponent::setObservable(bool observable)
    {
        if (this->observable != observable)
        {
            LOG_THIS(DOM);

            this->observable = observable;

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

    void PositionComponentSystem::pushChildrenInChange(std::set<_unique_id>& set, _unique_id parentId)
    {
        for (const auto& child : parentalMap[parentId])
        {
            auto inserted = set.insert(child);

            if (inserted.second)
                pushChildrenInChange(set, child);
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
                pushChildrenInChange(changedIds, event.id);
            }

            eventQueue.pop();
        }

        // std::set<_unique_id> modifiedIds;
        // std::set<_unique_id> impactedIds;

        // while (changedIds.size() > 0)
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
                    auto changed = pos->updatefromAnchor(*anchor);

                    anchorChanged |= changed;
                }

                ecsRef->sendEvent(EntityChangedEvent{id});

                if (anchorChanged)
                    ecsRef->sendEvent(PositionComponentChangedEvent{id});

                // modifiedIds.insert(id);

                // if (anchorChanged)
                    // impactedIds.insert(id);
            }

            // LOG_INFO("PositionComponentSystem", "Changed ids: " << changedIds.size() << ", modified ids: " << modifiedIds.size() << ", impacted ids: " << impactedIds.size());

            changedIds.clear();

            // changedIds = impactedIds;

            // impactedIds.clear();
        }

        // for (const auto& id : modifiedIds)
        // {
            // ecsRef->sendEvent(EntityChangedEvent{id});
        // }
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
}