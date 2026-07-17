#include "stdafx.h"

#include "TTFText.generated.h"

#include "ECS/entitysystem.h"

namespace pg
{

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
    if (areNotAlmostEqual(scale, value))
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

void TTFText::setWrap(const bool& value)
{
    if (wrap != value)
    {
        wrap = value;

        if (ecsRef)
        {
            ecsRef->sendEvent(TTFTextChangedEvent{entityId});
        }
    }
}

void TTFText::setSpacing(const float& value)
{
    if (areNotAlmostEqual(spacing, value))
    {
        spacing = value;

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
    serialize(archive, "wrap", value.wrap);
    serialize(archive, "spacing", value.spacing);

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
    defaultDeserialize(serializedString, "wrap", data.wrap);
    defaultDeserialize(serializedString, "spacing", data.spacing);

    return data;
}

} // namespace pg
