#include "stdafx.h"

#include "StrokeRect2DObject.generated.h"

#include "ECS/entitysystem.h"

namespace pg
{

void StrokeRect2DObject::setColors(const constant::Vector4D& value)
{
    if (colors != value)
    {
        colors = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(StrokeRect2DObjectChangedEvent{entityId});
        }
    }
}

void StrokeRect2DObject::setStrokeWidth(const float& value)
{
    if (pg::areNotAlmostEqual(strokeWidth, value))
    {
        strokeWidth = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(StrokeRect2DObjectChangedEvent{entityId});
        }
    }
}

void StrokeRect2DObject::setGap(const float& value)
{
    if (pg::areNotAlmostEqual(gap, value))
    {
        gap = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(StrokeRect2DObjectChangedEvent{entityId});
        }
    }
}

void StrokeRect2DObject::setDoubled(const bool& value)
{
    if (doubled != value)
    {
        doubled = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(StrokeRect2DObjectChangedEvent{entityId});
        }
    }
}

void StrokeRect2DObject::setCornerRadius(const float& value)
{
    if (pg::areNotAlmostEqual(cornerRadius, value))
    {
        cornerRadius = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(StrokeRect2DObjectChangedEvent{entityId});
        }
    }
}

void StrokeRect2DObject::setOpacity(float alpha)
{
    colors.w = alpha;
    if (ecsRef)
        ecsRef->sendEvent(StrokeRect2DObjectChangedEvent{entityId});
}

// Serialize function for StrokeRect2DObject
template <>
void serialize(Archive& archive, const StrokeRect2DObject& value)
{
    archive.startSerialization("StrokeRect2DObject");

    serialize(archive, "colors", value.colors);
    serialize(archive, "strokeWidth", value.strokeWidth);
    serialize(archive, "gap", value.gap);
    serialize(archive, "doubled", value.doubled);
    serialize(archive, "cornerRadius", value.cornerRadius);

    archive.endSerialization();
}

// Deserialize function for StrokeRect2DObject
template <>
StrokeRect2DObject deserialize(const UnserializedObject& serializedString)
{
    StrokeRect2DObject data;

    defaultDeserialize(serializedString, "colors", data.colors);
    defaultDeserialize(serializedString, "strokeWidth", data.strokeWidth);
    defaultDeserialize(serializedString, "gap", data.gap);
    defaultDeserialize(serializedString, "doubled", data.doubled);
    defaultDeserialize(serializedString, "cornerRadius", data.cornerRadius);

    return data;
}

} // namespace pg
