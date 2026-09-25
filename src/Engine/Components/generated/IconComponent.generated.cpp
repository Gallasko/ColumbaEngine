#include "stdafx.h"

#include "IconComponent.generated.h"

#include "ECS/entitysystem.h"

namespace pg
{

void IconComponent::setIconSet(const std::string& value)
{
    if (iconSet != value)
    {
        iconSet = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(IconChangedEvent{entityId});
        }
    }
}

void IconComponent::setIconName(const std::string& value)
{
    if (iconName != value)
    {
        iconName = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(IconChangedEvent{entityId});
        }
    }
}

void IconComponent::setColors(const constant::Vector4D& value)
{
    if (colors != value)
    {
        colors = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(IconChangedEvent{entityId});
        }
    }
}

// Serialize function for IconComponent
template <>
void serialize(Archive& archive, const IconComponent& value)
{
    archive.startSerialization("IconComponent");

    serialize(archive, "iconSet", value.iconSet);
    serialize(archive, "iconName", value.iconName);
    serialize(archive, "colors", value.colors);

    archive.endSerialization();
}

// Deserialize function for IconComponent
template <>
IconComponent deserialize(const UnserializedObject& serializedString)
{
    IconComponent data;

    defaultDeserialize(serializedString, "iconSet", data.iconSet);
    defaultDeserialize(serializedString, "iconName", data.iconName);
    defaultDeserialize(serializedString, "colors", data.colors);

    return data;
}

} // namespace pg
