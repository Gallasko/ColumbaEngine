#include "stdafx.h"

#include "PositionComponent.generated.h"

#include "ECS/entitysystem.h"

namespace pg
{

void PositionComponent::setX(const float& value)
{
    if (areNotAlmostEqual(x, value))
    {
        x = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }
}

void PositionComponent::setY(const float& value)
{
    if (areNotAlmostEqual(y, value))
    {
        y = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }
}

void PositionComponent::setZ(const float& value)
{
    if (areNotAlmostEqual(z, value))
    {
        z = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }
}

void PositionComponent::setWidth(const float& value)
{
    if (areNotAlmostEqual(width, value))
    {
        width = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }
}

void PositionComponent::setHeight(const float& value)
{
    if (areNotAlmostEqual(height, value))
    {
        height = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }
}

void PositionComponent::setRotation(const float& value)
{
    if (areNotAlmostEqual(rotation, value))
    {
        rotation = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }
}

void PositionComponent::setVisible(const bool& value)
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

void PositionComponent::setObservable(const bool& value)
{
    if (observable != value)
    {
        observable = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(PositionComponentChangedEvent{entityId});
        }
    }
}

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
    serialize(archive, "observable", value.observable);

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
    defaultDeserialize(serializedString, "observable", data.observable);

    return data;
}

} // namespace pg
