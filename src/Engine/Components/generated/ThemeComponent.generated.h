#pragma once

#include "ECS/component.h"
#include <string>

namespace pg
{

struct ThemeComponentChangedEvent
{
    pg::_unique_id id = 0;
};

struct ThemeComponent : public Component
{
    DEFAULT_COMPONENT_MEMBERS(ThemeComponent)

    std::string element = "";

    ThemeComponent(const std::string& element) : element(element) {}

    std::string getElement() const { return element; }

    void setElement(const std::string& value);

    inline static std::string getType() { return "ThemeComponent"; }
};

template <>
void serialize(Archive& archive, const ThemeComponent& value);

template <>
ThemeComponent deserialize(const UnserializedObject& serializedString);

} // namespace pg

// Force reference to serialization registration
// This ensures the .serialization.cpp file is linked and static initializers run
extern "C" void __init_ThemeComponent_registration();
namespace __force_link_impl {
    static struct __ThemeComponent_ForceLink {
        __ThemeComponent_ForceLink() {
            // Call init to force linking of the .serialization.cpp
            __init_ThemeComponent_registration();
        }
    } __ThemeComponent_force_link_instance;
}
