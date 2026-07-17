#pragma once

#include "ECS/component.h"
#include <string>
#include "pgconstant.h"

namespace pg
{

struct TextureChangedEvent
{
    _unique_id id = 0;
};

struct Texture2DComponent : public Component
{
    DEFAULT_COMPONENT_MEMBERS(Texture2DComponent)

    std::string textureName = "";
    float opacity = 1.0f;
    constant::Vector3D overlappingColor = {0.0f, 0.0f, 0.0f};
    float overlappingColorRatio = 0.0f;

    Texture2DComponent(const std::string& textureName) : textureName(textureName) {}

    std::string getTextureName() const { return textureName; }
    float getOpacity() const { return opacity; }
    constant::Vector3D getOverlappingColor() const { return overlappingColor; }
    float getOverlappingColorRatio() const { return overlappingColorRatio; }

    void setTextureName(const std::string& value);
    void setOpacity(const float& value);
    void setOverlappingColor(const constant::Vector3D& value);
    void setOverlappingColorRatio(const float& value);

    void setTexture(const std::string& textureName);
    void setOverlappingColor(const constant::Vector3D& color, float ratio);

    inline static std::string getType() { return "Texture2DComponent"; }
};

template <>
void serialize(Archive& archive, const Texture2DComponent& value);

template <>
Texture2DComponent deserialize(const UnserializedObject& serializedString);

} // namespace pg

// Force reference to serialization registration
// This ensures the .serialization.cpp file is linked and static initializers run
extern "C" void __init_Texture2DComponent_registration();
namespace __force_link_impl {
    static struct __Texture2DComponent_ForceLink {
        __Texture2DComponent_ForceLink() {
            // Call init to force linking of the .serialization.cpp
            __init_Texture2DComponent_registration();
        }
    } __Texture2DComponent_force_link_instance;
}
