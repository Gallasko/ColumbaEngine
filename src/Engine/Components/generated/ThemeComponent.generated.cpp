#include "stdafx.h"

#include "ThemeComponent.generated.h"

#include "ECS/entitysystem.h"

namespace pg
{

void ThemeComponent::setElement(const std::string& value)
{
    if (element != value)
    {
        element = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(ThemeComponentChangedEvent{entityId});
        }
    }
}

// Serialize function for ThemeComponent
template <>
void serialize(Archive& archive, const ThemeComponent& value)
{
    archive.startSerialization("ThemeComponent");

    serialize(archive, "element", value.element);

    archive.endSerialization();
}

// Deserialize function for ThemeComponent
template <>
ThemeComponent deserialize(const UnserializedObject& serializedString)
{
    ThemeComponent data;

    defaultDeserialize(serializedString, "element", data.element);

    return data;
}

} // namespace pg
