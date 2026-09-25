#include "stdafx.h"

#include "TTFText.generated.h"

#include "ECS/entitysystem.h"

namespace pg
{

template <>
void serialize(Archive& archive, const TextOverflow& value)
{
    serialize(archive, textOverflowToString.at(value));
}

template <>
TextOverflow deserialize(const UnserializedObject& serializedString)
{
    auto str = deserialize<std::string>(serializedString);
    auto it = stringToTextOverflow.find(str);
    if (it != stringToTextOverflow.end())
        return it->second;

    return TextOverflow::Grow; // fallback to first value
}

template <>
void serialize(Archive& archive, const TextAlign& value)
{
    serialize(archive, textAlignToString.at(value));
}

template <>
TextAlign deserialize(const UnserializedObject& serializedString)
{
    auto str = deserialize<std::string>(serializedString);
    auto it = stringToTextAlign.find(str);
    if (it != stringToTextAlign.end())
        return it->second;

    return TextAlign::Left; // fallback to first value
}

void TTFText::setText(const std::string& value)
{
    if (text != value)
    {
        text = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(TTFTextChangedEvent{entityId});
        }
    }
}

void TTFText::setFontPath(const std::string& value)
{
    if (fontPath != value)
    {
        fontPath = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(TTFTextChangedEvent{entityId});
        }
    }
}

void TTFText::setScale(const float& value)
{
    if (pg::areNotAlmostEqual(scale, value))
    {
        scale = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(TTFTextChangedEvent{entityId});
        }
    }
}

void TTFText::setColors(const constant::Vector4D& value)
{
    if (colors != value)
    {
        colors = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(TTFTextChangedEvent{entityId});
        }
    }
}

void TTFText::setOverflow(const TextOverflow& value)
{
    if (overflow != value)
    {
        overflow = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(TTFTextChangedEvent{entityId});
        }
    }
}

void TTFText::setAlign(const TextAlign& value)
{
    if (align != value)
    {
        align = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(TTFTextChangedEvent{entityId});
        }
    }
}

void TTFText::setMaxLines(const int& value)
{
    if (maxLines != value)
    {
        maxLines = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(TTFTextChangedEvent{entityId});
        }
    }
}

void TTFText::setSpacing(const float& value)
{
    if (pg::areNotAlmostEqual(spacing, value))
    {
        spacing = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(TTFTextChangedEvent{entityId});
        }
    }
}

void TTFText::setLetterSpacing(const float& value)
{
    if (pg::areNotAlmostEqual(letterSpacing, value))
    {
        letterSpacing = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(TTFTextChangedEvent{entityId});
        }
    }
}

// Serialize function for TTFText
template <>
void serialize(Archive& archive, const TTFText& value)
{
    archive.startSerialization("TTFText");

    serialize(archive, "text", value.text);
    serialize(archive, "fontPath", value.fontPath);
    serialize(archive, "scale", value.scale);
    serialize(archive, "colors", value.colors);
    serialize(archive, "overflow", textOverflowToString.at(value.overflow));
    serialize(archive, "align", textAlignToString.at(value.align));
    serialize(archive, "maxLines", value.maxLines);
    serialize(archive, "spacing", value.spacing);
    serialize(archive, "letterSpacing", value.letterSpacing);

    archive.endSerialization();
}

// Deserialize function for TTFText
template <>
TTFText deserialize(const UnserializedObject& serializedString)
{
    TTFText data;

    defaultDeserialize(serializedString, "text", data.text);
    defaultDeserialize(serializedString, "fontPath", data.fontPath);
    defaultDeserialize(serializedString, "scale", data.scale);
    defaultDeserialize(serializedString, "colors", data.colors);
    {
        std::string overflowStr;
        defaultDeserialize(serializedString, "overflow", overflowStr);
        auto overflowIt = stringToTextOverflow.find(overflowStr);
        if (overflowIt != stringToTextOverflow.end()) data.overflow = overflowIt->second;
    }
    {
        std::string alignStr;
        defaultDeserialize(serializedString, "align", alignStr);
        auto alignIt = stringToTextAlign.find(alignStr);
        if (alignIt != stringToTextAlign.end()) data.align = alignIt->second;
    }
    defaultDeserialize(serializedString, "maxLines", data.maxLines);
    defaultDeserialize(serializedString, "spacing", data.spacing);
    defaultDeserialize(serializedString, "letterSpacing", data.letterSpacing);

    return data;
}

} // namespace pg
