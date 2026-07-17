#include "stdafx.h"

#include "Texture2DComponent.generated.h"

#include "ECS/entitysystem.h"

namespace pg
{

void Texture2DComponent::setTextureName(const std::string& value)
{
    if (textureName != value)
    {
        textureName = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(TextureChangedEvent{entityId});
        }
    }
}

void Texture2DComponent::setOpacity(const float& value)
{
    if (areNotAlmostEqual(opacity, value))
    {
        opacity = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(TextureChangedEvent{entityId});
        }
    }
}

void Texture2DComponent::setOverlappingColor(const constant::Vector3D& value)
{
    if (overlappingColor != value)
    {
        overlappingColor = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(TextureChangedEvent{entityId});
        }
    }
}

void Texture2DComponent::setOverlappingColorRatio(const float& value)
{
    if (areNotAlmostEqual(overlappingColorRatio, value))
    {
        overlappingColorRatio = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(TextureChangedEvent{entityId});
        }
    }
}

// Serialize function for Texture2DComponent
template <>
void serialize(Archive& archive, const Texture2DComponent& value)
{
    archive.startSerialization("Texture2DComponent");

    serialize(archive, "textureName", value.textureName);
    serialize(archive, "opacity", value.opacity);
    serialize(archive, "overlappingColor", value.overlappingColor);
    serialize(archive, "overlappingColorRatio", value.overlappingColorRatio);

    archive.endSerialization();
}

// Deserialize function for Texture2DComponent
template <>
Texture2DComponent deserialize(const UnserializedObject& serializedString)
{
    Texture2DComponent data;

    defaultDeserialize(serializedString, "textureName", data.textureName);
    defaultDeserialize(serializedString, "opacity", data.opacity);
    defaultDeserialize(serializedString, "overlappingColor", data.overlappingColor);
    defaultDeserialize(serializedString, "overlappingColorRatio", data.overlappingColorRatio);

    return data;
}

} // namespace pg
