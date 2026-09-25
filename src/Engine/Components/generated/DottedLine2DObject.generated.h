#pragma once

#include "ECS/component.h"
#include "pgconstant.h"

namespace pg
{

struct DottedLine2DObjectChangedEvent
{
    pg::_unique_id id = 0;
};

struct DottedLine2DObject : public Component
{
    DEFAULT_COMPONENT_MEMBERS(DottedLine2DObject)

    constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f};
    float period = 4.0f;
    float dotRadius = 0.75f;

    DottedLine2DObject(const constant::Vector4D& colors) : colors(colors) {}
    DottedLine2DObject(const constant::Vector4D& colors, const float& period, const float& dotRadius) : colors(colors), period(period), dotRadius(dotRadius) {}

    constant::Vector4D getColors() const { return colors; }
    float getPeriod() const { return period; }
    float getDotRadius() const { return dotRadius; }

    void setColors(const constant::Vector4D& value);
    void setPeriod(const float& value);
    void setDotRadius(const float& value);

    void setOpacity(float alpha);

    inline static std::string getType() { return "DottedLine2DObject"; }
};

template <>
void serialize(Archive& archive, const DottedLine2DObject& value);

template <>
DottedLine2DObject deserialize(const UnserializedObject& serializedString);

} // namespace pg

// Force reference to serialization registration
// This ensures the .serialization.cpp file is linked and static initializers run
extern "C" void __init_DottedLine2DObject_registration();
namespace __force_link_impl {
    static struct __DottedLine2DObject_ForceLink {
        __DottedLine2DObject_ForceLink() {
            // Call init to force linking of the .serialization.cpp
            __init_DottedLine2DObject_registration();
        }
    } __DottedLine2DObject_force_link_instance;
}
