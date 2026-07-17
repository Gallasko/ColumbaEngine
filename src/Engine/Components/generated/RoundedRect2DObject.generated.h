#pragma once

#include "ECS/component.h"
#include "pgconstant.h"

namespace pg
{

struct RoundedRect2DObjectChangedEvent
{
    _unique_id id = 0;
};

struct RoundedRect2DObject : public Component
{
    DEFAULT_COMPONENT_MEMBERS(RoundedRect2DObject)

    float cornerRadius = 0.0f;
    constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f};

    RoundedRect2DObject(const float& cornerRadius) : cornerRadius(cornerRadius) {}
    RoundedRect2DObject(const float& cornerRadius, const constant::Vector4D& colors = {255.0f, 255.0f, 255.0f, 255.0f}) : cornerRadius(cornerRadius), colors(colors) {}

    float getCornerRadius() const { return cornerRadius; }
    constant::Vector4D getColors() const { return colors; }

    void setCornerRadius(const float& value);
    void setColors(const constant::Vector4D& value);

    void setOpacity(float alpha);

    inline static std::string getType() { return "RoundedRect2DObject"; }
};

template <>
void serialize(Archive& archive, const RoundedRect2DObject& value);

template <>
RoundedRect2DObject deserialize(const UnserializedObject& serializedString);

} // namespace pg

// Force reference to serialization registration
// This ensures the .serialization.cpp file is linked and static initializers run
extern "C" void __init_RoundedRect2DObject_registration();
namespace __force_link_impl {
    static struct __RoundedRect2DObject_ForceLink {
        __RoundedRect2DObject_ForceLink() {
            // Call init to force linking of the .serialization.cpp
            __init_RoundedRect2DObject_registration();
        }
    } __RoundedRect2DObject_force_link_instance;
}
