#include "stdafx.h"

#include "configuration.h"

#include "logger.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Configuration";
    }

    template <>
    void serialize(Archive& archive, const Configuration& config)
    {
        LOG_THIS(DOM);

        archive.startSerialization("Configuration");

        for(const auto& element : config.elementMap)
        {
            serialize(archive, element.first, element.second);
        }

        archive.endSerialization();
    }

    template <>
    Configuration deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        Configuration value;

        for(auto& element : serializedString.children)
        {
            if (element.isNull())
            {
                LOG_ERROR(DOM, "Element is null");
            }
            else if (element.getObjectName() == "")
            {
                LOG_ERROR(DOM, "Element has no name");
            }
            else
            {
                LOG_INFO(DOM, "Deserializing " + element.getObjectName());

                auto child = deserialize<ElementType>(element);
                value.elementMap[element.getObjectName()] = child;
            }
        }

        return value;
    }
}