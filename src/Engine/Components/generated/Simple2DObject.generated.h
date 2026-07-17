#pragma once

#include "ECS/component.h"
#include "pgconstant.h"
#include "Helpers/helpers.h"

namespace pg
{

enum class Shape2D : uint8_t
{
    Triangle = 0,
    Square,
    Circle,
    None,
};

const static std::unordered_map<Shape2D, std::string> shape2DToString = {
    {Shape2D::Triangle, "Triangle"},
    {Shape2D::Square, "Square"},
    {Shape2D::Circle, "Circle"},
    {Shape2D::None, "None"},
};

const static auto stringToShape2D = invertMap(shape2DToString);

template <>
void serialize(Archive& archive, const Shape2D& value);

template <>
Shape2D deserialize(const UnserializedObject& serializedString);

struct Simple2DObjectChangedEvent
{
    _unique_id id = 0;
};

struct Simple2DObject : public Component
{
    DEFAULT_COMPONENT_MEMBERS(Simple2DObject)

    Shape2D shape = Shape2D::Triangle;
    constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f};

    Simple2DObject(const Shape2D& shape) : shape(shape) {}
    Simple2DObject(const Shape2D& shape, const constant::Vector4D& colors) : shape(shape), colors(colors) {}

    Shape2D getShape() const { return shape; }
    constant::Vector4D getColors() const { return colors; }

    void setShape(const Shape2D& value);
    void setColors(const constant::Vector4D& value);

    void setOpacity(float alpha);

    inline static std::string getType() { return "Simple2DObject"; }
};

template <>
void serialize(Archive& archive, const Simple2DObject& value);

template <>
Simple2DObject deserialize(const UnserializedObject& serializedString);

} // namespace pg

// Force reference to serialization registration
// This ensures the .serialization.cpp file is linked and static initializers run
extern "C" void __init_Simple2DObject_registration();
namespace __force_link_impl {
    static struct __Simple2DObject_ForceLink {
        __Simple2DObject_ForceLink() {
            // Call init to force linking of the .serialization.cpp
            __init_Simple2DObject_registration();
        }
    } __Simple2DObject_force_link_instance;
}
