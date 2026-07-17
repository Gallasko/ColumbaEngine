#pragma once

#include "ECS/component.h"
#include <string>
#include "pgconstant.h"

namespace pg
{

struct TTFTextChangedEvent
{
    _unique_id id = 0;
};

struct TTFText : public Component
{
    DEFAULT_COMPONENT_MEMBERS(TTFText)

    std::string text = "";
    float textWidth = 0.0f;
    float textHeight = 0.0f;
    std::string fontPath = "";
    float scale = 1.0f;
    constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f};
    bool wrap = false;
    float spacing = 0.0f;
    bool changed = false;

    TTFText(const std::string& text, const std::string& fontPath, const float& scale, const constant::Vector4D& colors = {255.0f, 255.0f, 255.0f, 255.0f}) : text(text), fontPath(fontPath), scale(scale), colors(colors) {}

    std::string getText() const { return text; }
    float getTextWidth() const { return textWidth; }
    float getTextHeight() const { return textHeight; }
    std::string getFontPath() const { return fontPath; }
    float getScale() const { return scale; }
    constant::Vector4D getColors() const { return colors; }
    bool getWrap() const { return wrap; }
    float getSpacing() const { return spacing; }
    bool getChanged() const { return changed; }

    void setText(const std::string& value);
    void setFontPath(const std::string& value);
    void setScale(const float& value);
    void setColors(const constant::Vector4D& value);
    void setWrap(const bool& value);
    void setSpacing(const float& value);

    inline static std::string getType() { return "TTFText"; }
};

template <>
void serialize(Archive& archive, const TTFText& value);

template <>
TTFText deserialize(const UnserializedObject& serializedString);

} // namespace pg

// Force reference to serialization registration
// This ensures the .serialization.cpp file is linked and static initializers run
extern "C" void __init_TTFText_registration();
namespace __force_link_impl {
    static struct __TTFText_ForceLink {
        __TTFText_ForceLink() {
            // Call init to force linking of the .serialization.cpp
            __init_TTFText_registration();
        }
    } __TTFText_force_link_instance;
}
