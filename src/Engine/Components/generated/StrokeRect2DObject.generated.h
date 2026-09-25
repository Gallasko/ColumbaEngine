#pragma once

#include "ECS/component.h"
#include "pgconstant.h"

namespace pg
{

struct StrokeRect2DObjectChangedEvent
{
    pg::_unique_id id = 0;
};

struct StrokeRect2DObject : public Component
{
    DEFAULT_COMPONENT_MEMBERS(StrokeRect2DObject)

    constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f};
    float strokeWidth = 1.0f;
    float gap = 0.0f;
    bool doubled = false;
    float cornerRadius = 0.0f;

    StrokeRect2DObject(const constant::Vector4D& colors) : colors(colors) {}
    StrokeRect2DObject(const constant::Vector4D& colors, const float& strokeWidth) : colors(colors), strokeWidth(strokeWidth) {}
    StrokeRect2DObject(const constant::Vector4D& colors, const float& strokeWidth, const float& gap, const bool& doubled) : colors(colors), strokeWidth(strokeWidth), gap(gap), doubled(doubled) {}

    constant::Vector4D getColors() const { return colors; }
    float getStrokeWidth() const { return strokeWidth; }
    float getGap() const { return gap; }
    bool getDoubled() const { return doubled; }
    float getCornerRadius() const { return cornerRadius; }

    void setColors(const constant::Vector4D& value);
    void setStrokeWidth(const float& value);
    void setGap(const float& value);
    void setDoubled(const bool& value);
    void setCornerRadius(const float& value);

    void setOpacity(float alpha);

    inline static std::string getType() { return "StrokeRect2DObject"; }
};

template <>
void serialize(Archive& archive, const StrokeRect2DObject& value);

template <>
StrokeRect2DObject deserialize(const UnserializedObject& serializedString);

} // namespace pg

// Force reference to serialization registration
// This ensures the .serialization.cpp file is linked and static initializers run
extern "C" void __init_StrokeRect2DObject_registration();
namespace __force_link_impl {
    static struct __StrokeRect2DObject_ForceLink {
        __StrokeRect2DObject_ForceLink() {
            // Call init to force linking of the .serialization.cpp
            __init_StrokeRect2DObject_registration();
        }
    } __StrokeRect2DObject_force_link_instance;
}
