#include "stdafx.h"

#include "DottedLine2DObject.generated.h"

#include "ECS/entitysystem.h"

namespace pg
{

void DottedLine2DObject::setColors(const constant::Vector4D& value)
{
    if (colors != value)
    {
        colors = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(DottedLine2DObjectChangedEvent{entityId});
        }
    }
}

void DottedLine2DObject::setPeriod(const float& value)
{
    if (pg::areNotAlmostEqual(period, value))
    {
        period = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(DottedLine2DObjectChangedEvent{entityId});
        }
    }
}

void DottedLine2DObject::setDotRadius(const float& value)
{
    if (pg::areNotAlmostEqual(dotRadius, value))
    {
        dotRadius = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(DottedLine2DObjectChangedEvent{entityId});
        }
    }
}

void DottedLine2DObject::setOpacity(float alpha)
{
    colors.w = alpha;
    if (ecsRef)
        ecsRef->sendEvent(DottedLine2DObjectChangedEvent{entityId});
}

// Serialize function for DottedLine2DObject
template <>
void serialize(Archive& archive, const DottedLine2DObject& value)
{
    archive.startSerialization("DottedLine2DObject");

    serialize(archive, "colors", value.colors);
    serialize(archive, "period", value.period);
    serialize(archive, "dotRadius", value.dotRadius);

    archive.endSerialization();
}

// Deserialize function for DottedLine2DObject
template <>
DottedLine2DObject deserialize(const UnserializedObject& serializedString)
{
    DottedLine2DObject data;

    defaultDeserialize(serializedString, "colors", data.colors);
    defaultDeserialize(serializedString, "period", data.period);
    defaultDeserialize(serializedString, "dotRadius", data.dotRadius);

    return data;
}

} // namespace pg
