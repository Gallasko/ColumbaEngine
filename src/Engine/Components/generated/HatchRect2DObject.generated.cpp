#include "stdafx.h"

#include "HatchRect2DObject.generated.h"

#include "ECS/entitysystem.h"

namespace pg
{

void HatchRect2DObject::setColors(const constant::Vector4D& value)
{
    if (colors != value)
    {
        colors = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(HatchRect2DObjectChangedEvent{entityId});
        }
    }
}

void HatchRect2DObject::setSpacing(const float& value)
{
    if (pg::areNotAlmostEqual(spacing, value))
    {
        spacing = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(HatchRect2DObjectChangedEvent{entityId});
        }
    }
}

void HatchRect2DObject::setLineWidth(const float& value)
{
    if (pg::areNotAlmostEqual(lineWidth, value))
    {
        lineWidth = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(HatchRect2DObjectChangedEvent{entityId});
        }
    }
}

void HatchRect2DObject::setAngle(const float& value)
{
    if (pg::areNotAlmostEqual(angle, value))
    {
        angle = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(HatchRect2DObjectChangedEvent{entityId});
        }
    }
}

void HatchRect2DObject::setCornerRadius(const float& value)
{
    if (pg::areNotAlmostEqual(cornerRadius, value))
    {
        cornerRadius = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(HatchRect2DObjectChangedEvent{entityId});
        }
    }
}

void HatchRect2DObject::setOpacity(float alpha)
{
    colors.w = alpha;
    if (ecsRef)
        ecsRef->sendEvent(HatchRect2DObjectChangedEvent{entityId});
}

// Serialize function for HatchRect2DObject
template <>
void serialize(Archive& archive, const HatchRect2DObject& value)
{
    archive.startSerialization("HatchRect2DObject");

    serialize(archive, "colors", value.colors);
    serialize(archive, "spacing", value.spacing);
    serialize(archive, "lineWidth", value.lineWidth);
    serialize(archive, "angle", value.angle);
    serialize(archive, "cornerRadius", value.cornerRadius);

    archive.endSerialization();
}

// Deserialize function for HatchRect2DObject
template <>
HatchRect2DObject deserialize(const UnserializedObject& serializedString)
{
    HatchRect2DObject data;

    defaultDeserialize(serializedString, "colors", data.colors);
    defaultDeserialize(serializedString, "spacing", data.spacing);
    defaultDeserialize(serializedString, "lineWidth", data.lineWidth);
    defaultDeserialize(serializedString, "angle", data.angle);
    defaultDeserialize(serializedString, "cornerRadius", data.cornerRadius);

    return data;
}

} // namespace pg
