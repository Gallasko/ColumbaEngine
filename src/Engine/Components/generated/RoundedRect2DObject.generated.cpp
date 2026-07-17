#include "stdafx.h"

#include "RoundedRect2DObject.generated.h"

#include "ECS/entitysystem.h"

namespace pg
{

void RoundedRect2DObject::setCornerRadius(const float& value)
{
    if (areNotAlmostEqual(cornerRadius, value))
    {
        cornerRadius = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(RoundedRect2DObjectChangedEvent{entityId});
        }
    }
}

void RoundedRect2DObject::setColors(const constant::Vector4D& value)
{
    if (colors != value)
    {
        colors = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(RoundedRect2DObjectChangedEvent{entityId});
        }
    }
}

void RoundedRect2DObject::setOpacity(float alpha)
{
    colors.w = alpha;
    if (ecsRef)
        ecsRef->sendEvent(RoundedRect2DObjectChangedEvent{entityId});
}

// Serialize function for RoundedRect2DObject
template <>
void serialize(Archive& archive, const RoundedRect2DObject& value)
{
    archive.startSerialization("RoundedRect2DObject");

    serialize(archive, "cornerRadius", value.cornerRadius);
    serialize(archive, "colors", value.colors);

    archive.endSerialization();
}

// Deserialize function for RoundedRect2DObject
template <>
RoundedRect2DObject deserialize(const UnserializedObject& serializedString)
{
    RoundedRect2DObject data;

    defaultDeserialize(serializedString, "cornerRadius", data.cornerRadius);
    defaultDeserialize(serializedString, "colors", data.colors);

    return data;
}

} // namespace pg
