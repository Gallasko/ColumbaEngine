#include "stdafx.h"

#include "ViewportComponent.generated.h"

#include "ECS/entitysystem.h"

namespace pg
{

void ViewportComponent::setViewport(const size_t& value)
{
    if (viewport != value)
    {
        viewport = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(ViewportComponentChangedEvent{entityId});
        }
    }
}

// Serialize function for ViewportComponent
template <>
void serialize(Archive& archive, const ViewportComponent& value)
{
    archive.startSerialization("ViewportComponent");

    serialize(archive, "viewport", value.viewport);

    archive.endSerialization();
}

// Deserialize function for ViewportComponent
template <>
ViewportComponent deserialize(const UnserializedObject& serializedString)
{
    ViewportComponent data;

    defaultDeserialize(serializedString, "viewport", data.viewport);

    return data;
}

} // namespace pg
