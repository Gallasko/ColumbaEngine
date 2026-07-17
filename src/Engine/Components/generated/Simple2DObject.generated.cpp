#include "stdafx.h"

#include "Simple2DObject.generated.h"

#include "ECS/entitysystem.h"

namespace pg
{

template <>
void serialize(Archive& archive, const Shape2D& value)
{
    serialize(archive, shape2DToString.at(value));
}

template <>
Shape2D deserialize(const UnserializedObject& serializedString)
{
    auto str = deserialize<std::string>(serializedString);
    auto it = stringToShape2D.find(str);
    if (it != stringToShape2D.end()) return it->second;
    return Shape2D::Triangle; // fallback to first value
}

void Simple2DObject::setShape(const Shape2D& value)
{
    if (shape != value)
    {
        shape = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(Simple2DObjectChangedEvent{entityId});
        }
    }
}

void Simple2DObject::setColors(const constant::Vector4D& value)
{
    if (colors != value)
    {
        colors = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(Simple2DObjectChangedEvent{entityId});
        }
    }
}

void Simple2DObject::setOpacity(float alpha)
{
    colors.w = alpha;
    if (ecsRef)
    {
        ecsRef->sendEvent(Simple2DObjectChangedEvent{entityId});
    }
}

// Serialize function for Simple2DObject
template <>
void serialize(Archive& archive, const Simple2DObject& value)
{
    archive.startSerialization("Simple2DObject");

    serialize(archive, "shape", shape2DToString.at(value.shape));
    serialize(archive, "colors", value.colors);

    archive.endSerialization();
}

// Deserialize function for Simple2DObject
template <>
Simple2DObject deserialize(const UnserializedObject& serializedString)
{
    Simple2DObject data;

    {
        std::string shapeStr;
        defaultDeserialize(serializedString, "shape", shapeStr);
        auto shapeIt = stringToShape2D.find(shapeStr);
        if (shapeIt != stringToShape2D.end()) data.shape = shapeIt->second;
    }
    defaultDeserialize(serializedString, "colors", data.colors);

    return data;
}

} // namespace pg
