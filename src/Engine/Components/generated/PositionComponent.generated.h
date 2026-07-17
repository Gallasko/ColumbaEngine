#pragma once

#include "ECS/component.h"

namespace pg
{

struct UiAnchor;

struct PositionComponentChangedEvent
{
    _unique_id id = 0;
};

struct PositionComponent : public Component
{
    DEFAULT_COMPONENT_MEMBERS(PositionComponent)

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float rotation = 0.0f;
    bool visible = true;
    bool observable = true;

    float getX() const { return x; }
    float getY() const { return y; }
    float getZ() const { return z; }
    float getWidth() const { return width; }
    float getHeight() const { return height; }
    float getRotation() const { return rotation; }
    bool getVisible() const { return visible; }
    bool getObservable() const { return observable; }

    void setX(const float& value);
    void setY(const float& value);
    void setZ(const float& value);
    void setWidth(const float& value);
    void setHeight(const float& value);
    void setRotation(const float& value);
    void setVisible(const bool& value);
    void setObservable(const bool& value);

    bool isVisible() const { return visible; }
    bool isObservable() const { return observable; }
    bool isRenderable() const { return visible and observable; }
    bool updatefromAnchor(const UiAnchor& anchor);
    void setVisibility(const bool& value);

    inline static std::string getType() { return "PositionComponent"; }
};

template <>
void serialize(Archive& archive, const PositionComponent& value);

template <>
PositionComponent deserialize(const UnserializedObject& serializedString);

} // namespace pg

// Force reference to serialization registration
// This ensures the .serialization.cpp file is linked and static initializers run
extern "C" void __init_PositionComponent_registration();
namespace __force_link_impl {
    static struct __PositionComponent_ForceLink {
        __PositionComponent_ForceLink() {
            // Call init to force linking of the .serialization.cpp
            __init_PositionComponent_registration();
        }
    } __PositionComponent_force_link_instance;
}
