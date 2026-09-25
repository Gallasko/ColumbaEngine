#pragma once

#include "ECS/component.h"
#include "pgconstant.h"

namespace pg
{

struct HatchRect2DObjectChangedEvent
{
    pg::_unique_id id = 0;
};

struct HatchRect2DObject : public Component
{
    DEFAULT_COMPONENT_MEMBERS(HatchRect2DObject)

    constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f};
    float spacing = 6.0f;
    float lineWidth = 2.0f;
    float angle = 45.0f;
    float cornerRadius = 0.0f;

    HatchRect2DObject(const constant::Vector4D& colors) : colors(colors) {}
    HatchRect2DObject(const constant::Vector4D& colors, const float& spacing, const float& lineWidth) : colors(colors), spacing(spacing), lineWidth(lineWidth) {}

    constant::Vector4D getColors() const { return colors; }
    float getSpacing() const { return spacing; }
    float getLineWidth() const { return lineWidth; }
    float getAngle() const { return angle; }
    float getCornerRadius() const { return cornerRadius; }

    void setColors(const constant::Vector4D& value);
    void setSpacing(const float& value);
    void setLineWidth(const float& value);
    void setAngle(const float& value);
    void setCornerRadius(const float& value);

    void setOpacity(float alpha);

    inline static std::string getType() { return "HatchRect2DObject"; }
};

template <>
void serialize(Archive& archive, const HatchRect2DObject& value);

template <>
HatchRect2DObject deserialize(const UnserializedObject& serializedString);

} // namespace pg

// Force reference to serialization registration
// This ensures the .serialization.cpp file is linked and static initializers run
extern "C" void __init_HatchRect2DObject_registration();
namespace __force_link_impl {
    static struct __HatchRect2DObject_ForceLink {
        __HatchRect2DObject_ForceLink() {
            // Call init to force linking of the .serialization.cpp
            __init_HatchRect2DObject_registration();
        }
    } __HatchRect2DObject_force_link_instance;
}
