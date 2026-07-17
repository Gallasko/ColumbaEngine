#pragma once

#include "ECS/component.h"

namespace pg
{

struct ViewportComponentChangedEvent
{
    _unique_id id = 0;
};

struct ViewportComponent : public Component
{
    DEFAULT_COMPONENT_MEMBERS(ViewportComponent)

    size_t viewport = 0;

    size_t getViewport() const { return viewport; }

    void setViewport(const size_t& value);

    inline static std::string getType() { return "ViewportComponent"; }
};

template <>
void serialize(Archive& archive, const ViewportComponent& value);

template <>
ViewportComponent deserialize(const UnserializedObject& serializedString);

} // namespace pg

// Force reference to serialization registration
// This ensures the .serialization.cpp file is linked and static initializers run
extern "C" void __init_ViewportComponent_registration();
namespace __force_link_impl {
    static struct __ViewportComponent_ForceLink {
        __ViewportComponent_ForceLink() {
            // Call init to force linking of the .serialization.cpp
            __init_ViewportComponent_registration();
        }
    } __ViewportComponent_force_link_instance;
}
