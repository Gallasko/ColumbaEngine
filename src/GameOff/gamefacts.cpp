#include "gamefacts.h"

namespace pg
{
    template <>
    void serialize(Archive& archive, const FactChecker& value)
    {
        archive.startSerialization("FactChecker");
        
        serialize(archive, "name", value.name);
        serialize(archive, "value", value.value);
        serialize(archive, "equality", equalityToString.at(value.equality));
        
        archive.endSerialization();
    }

    template <>
    FactChecker deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS("Fact checker");

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("Fact checker", "Element is null");
        }
        else
        {
            LOG_INFO("Fact checker", "Deserializing FactChecker");

            FactChecker data;

            defaultDeserialize(serializedString, "name", data.name);
            defaultDeserialize(serializedString, "value", data.value);

            std::string equality = "None";
            defaultDeserialize(serializedString, "equality", equality);

            data.equality = stringToEquality.at(equality);
            
            LOG_INFO("Fact checker", "FactChecker deserialized");

            return data;
        }

        return FactChecker{};
    }
}