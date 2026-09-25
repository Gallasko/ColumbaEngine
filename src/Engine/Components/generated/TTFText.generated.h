#pragma once

#include "ECS/component.h"
#include <string>
#include "pgconstant.h"
#include "Helpers/helpers.h"

namespace pg
{

enum class TextOverflow : uint8_t
{
    Grow = 0,
    Wrap,
    Ellipsis,
};

const static std::unordered_map<TextOverflow, std::string> textOverflowToString = {
    {TextOverflow::Grow, "Grow"},
    {TextOverflow::Wrap, "Wrap"},
    {TextOverflow::Ellipsis, "Ellipsis"},
};

const static auto stringToTextOverflow = invertMap(textOverflowToString);

template <>
void serialize(Archive& archive, const TextOverflow& value);

template <>
TextOverflow deserialize(const UnserializedObject& serializedString);

enum class TextAlign : uint8_t
{
    Left = 0,
    Centre,
    Right,
};

const static std::unordered_map<TextAlign, std::string> textAlignToString = {
    {TextAlign::Left, "Left"},
    {TextAlign::Centre, "Centre"},
    {TextAlign::Right, "Right"},
};

const static auto stringToTextAlign = invertMap(textAlignToString);

template <>
void serialize(Archive& archive, const TextAlign& value);

template <>
TextAlign deserialize(const UnserializedObject& serializedString);

struct TTFTextChangedEvent
{
    pg::_unique_id id = 0;
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
    TextOverflow overflow = TextOverflow::Grow;
    TextAlign align = TextAlign::Left;
    int maxLines = 0;
    float spacing = 0.0f;
    float letterSpacing = 0.0f;
    bool changed = false;

    TTFText(const std::string& text, const std::string& fontPath, const float& scale, const constant::Vector4D& colors = {255.0f, 255.0f, 255.0f, 255.0f}) : text(text), fontPath(fontPath), scale(scale), colors(colors) {}

    std::string getText() const { return text; }
    float getTextWidth() const { return textWidth; }
    float getTextHeight() const { return textHeight; }
    std::string getFontPath() const { return fontPath; }
    float getScale() const { return scale; }
    constant::Vector4D getColors() const { return colors; }
    TextOverflow getOverflow() const { return overflow; }
    TextAlign getAlign() const { return align; }
    int getMaxLines() const { return maxLines; }
    float getSpacing() const { return spacing; }
    float getLetterSpacing() const { return letterSpacing; }
    bool getChanged() const { return changed; }

    void setText(const std::string& value);
    void setFontPath(const std::string& value);
    void setScale(const float& value);
    void setColors(const constant::Vector4D& value);
    void setOverflow(const TextOverflow& value);
    void setAlign(const TextAlign& value);
    void setMaxLines(const int& value);
    void setSpacing(const float& value);
    void setLetterSpacing(const float& value);

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
