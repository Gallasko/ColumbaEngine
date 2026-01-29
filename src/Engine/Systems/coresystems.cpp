#include "stdafx.h"

#include "coresystems.h"

namespace pg
{
    static constexpr char const * DOM = "Core System";

    template <>
    void serialize(Archive& archive, const EntityName& value)
    {
        LOG_THIS(DOM);

        archive.startSerialization(EntityName::getType());

        serialize(archive, "entityName", value.name);

        archive.endSerialization();
    }

    template <>
    EntityName deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing an EntityName");

            auto entityName = deserialize<std::string>(serializedString["entityName"]);

            return EntityName{entityName};
        }

        return EntityName{""};
    }
}