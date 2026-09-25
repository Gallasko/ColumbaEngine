#pragma once

#include "ECS/component.h"
#include <string>
#include "pgconstant.h"

namespace pg
{

struct IconChangedEvent
{
    pg::_unique_id id = 0;
};

struct IconComponent : public Component
{
    DEFAULT_COMPONENT_MEMBERS(IconComponent)

    std::string iconSet = "";
    std::string iconName = "";
    constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f};

    IconComponent(const std::string& iconSet, const std::string& iconName) : iconSet(iconSet), iconName(iconName) {}
    IconComponent(const std::string& iconSet, const std::string& iconName, const constant::Vector4D& colors) : iconSet(iconSet), iconName(iconName), colors(colors) {}

    std::string getIconSet() const { return iconSet; }
    std::string getIconName() const { return iconName; }
    constant::Vector4D getColors() const { return colors; }

    void setIconSet(const std::string& value);
    void setIconName(const std::string& value);
    void setColors(const constant::Vector4D& value);

    inline static std::string getType() { return "IconComponent"; }
};

template <>
void serialize(Archive& archive, const IconComponent& value);

template <>
IconComponent deserialize(const UnserializedObject& serializedString);

} // namespace pg

// Force reference to serialization registration
// This ensures the .serialization.cpp file is linked and static initializers run
extern "C" void __init_IconComponent_registration();
namespace __force_link_impl {
    static struct __IconComponent_ForceLink {
        __IconComponent_ForceLink() {
            // Call init to force linking of the .serialization.cpp
            __init_IconComponent_registration();
        }
    } __IconComponent_force_link_instance;
}
